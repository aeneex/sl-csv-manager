#include "schema_transformer.hpp"
#include <iostream>

namespace sl {

SchemaTransformer::SchemaTransformer(SchemaConfig config)
    : config_(std::move(config)) {
    build_alias_lookup();
}

void SchemaTransformer::set_config(const SchemaConfig& config) {
    config_ = config;
    build_alias_lookup();
}

const SchemaConfig& SchemaTransformer::get_config() const {
    return config_;
}

void SchemaTransformer::build_alias_lookup() {
    alias_lookup_.clear();

    // Map each target header directly (self-mapping)
    for (const auto& target : config_.target_headers) {
        std::string clean_target = CSVReader::to_lower(CSVReader::trim(target));
        alias_lookup_[clean_target] = clean_target;
    }

    // Map all configured aliases
    for (const auto& [target, aliases] : config_.mappings) {
        std::string clean_target = CSVReader::to_lower(CSVReader::trim(target));
        for (const auto& alias : aliases) {
            std::string clean_alias = CSVReader::to_lower(CSVReader::trim(alias));
            if (!clean_alias.empty()) {
                alias_lookup_[clean_alias] = clean_target;
            }
        }
    }
}

std::unordered_map<std::string, size_t> SchemaTransformer::build_column_map(
    const std::vector<std::string>& source_headers) const {
    
    std::unordered_map<std::string, size_t> col_map;

    for (size_t j = 0; j < source_headers.size(); ++j) {
        std::string clean_header = CSVReader::to_lower(CSVReader::trim(source_headers[j]));
        if (clean_header.empty()) continue;

        std::string technical_name = clean_header;
        auto it = alias_lookup_.find(clean_header);
        if (it != alias_lookup_.end()) {
            technical_name = it->second;
        }

        col_map[technical_name] = j;
    }

    return col_map;
}

std::vector<std::vector<std::string>> SchemaTransformer::transform_rows(
    const std::vector<std::vector<std::string>>& input_rows,
    TransformStats* stats,
    bool drop_empty_columns) const {

    std::vector<std::vector<std::string>> output;
    if (input_rows.empty()) {
        return output;
    }

    const auto& source_headers = input_rows[0];
    auto col_map = build_column_map(source_headers);

    // Precompute source indices for each target header
    std::vector<int64_t> source_indices(config_.target_headers.size(), -1);
    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        std::string clean_target = CSVReader::to_lower(CSVReader::trim(config_.target_headers[c]));
        auto it = col_map.find(clean_target);
        if (it != col_map.end()) {
            source_indices[c] = static_cast<int64_t>(it->second);
        }
    }

    // Determine which target columns have at least one non-empty value
    std::vector<size_t> active_indices;
    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        int64_t src_idx = source_indices[c];
        if (src_idx < 0) {
            if (!drop_empty_columns) active_indices.push_back(c);
            continue;
        }

        if (!drop_empty_columns || input_rows.size() <= 1) {
            active_indices.push_back(c);
            continue;
        }

        bool has_data = false;
        for (size_t r = 1; r < input_rows.size(); ++r) {
            if (static_cast<size_t>(src_idx) < input_rows[r].size()) {
                if (!CSVReader::trim(input_rows[r][src_idx]).empty()) {
                    has_data = true;
                    break;
                }
            }
        }

        if (has_data) {
            active_indices.push_back(c);
        }
    }

    if (stats) {
        stats->total_target_columns = config_.target_headers.size();
        stats->mapped_columns.clear();
        stats->unmapped_target_columns.clear();
        stats->rows_processed = (input_rows.size() > 1) ? input_rows.size() - 1 : 0;

        for (size_t c = 0; c < config_.target_headers.size(); ++c) {
            if (source_indices[c] >= 0) {
                stats->mapped_columns.push_back(config_.target_headers[c]);
            } else {
                stats->unmapped_target_columns.push_back(config_.target_headers[c]);
            }
        }
        stats->columns_mapped = stats->mapped_columns.size();
    }

    // 1. Target Header Row
    std::vector<std::string> final_headers;
    for (size_t idx : active_indices) {
        final_headers.push_back(config_.target_headers[idx]);
    }
    output.push_back(final_headers);

    // 2. Transformed Data Rows
    for (size_t r = 1; r < input_rows.size(); ++r) {
        const auto& in_row = input_rows[r];
        std::vector<std::string> out_row;
        out_row.reserve(active_indices.size());

        for (size_t idx : active_indices) {
            int64_t src_idx = source_indices[idx];
            if (src_idx >= 0 && static_cast<size_t>(src_idx) < in_row.size()) {
                out_row.push_back(in_row[static_cast<size_t>(src_idx)]);
            } else {
                out_row.push_back("");
            }
        }
        output.push_back(std::move(out_row));
    }

    return output;
}

bool SchemaTransformer::transform_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    TransformStats* stats,
    bool drop_empty_columns) const {

    std::vector<std::string> source_headers;
    std::vector<std::vector<std::string>> data_rows;

    {
        CSVReader reader(input_path);
        if (!reader.is_open()) {
            return false;
        }

        if (!reader.read_row(source_headers)) {
            return false; // Empty file
        }

        std::vector<std::string> row;
        while (reader.read_row(row)) {
            if (row.size() == 1 && row[0].empty()) continue;
            data_rows.push_back(std::move(row));
        }
    }

    auto col_map = build_column_map(source_headers);

    // Precompute source indices for the target headers
    std::vector<int64_t> source_indices(config_.target_headers.size(), -1);
    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        std::string clean_target = CSVReader::to_lower(CSVReader::trim(config_.target_headers[c]));
        auto it = col_map.find(clean_target);
        if (it != col_map.end()) {
            source_indices[c] = static_cast<int64_t>(it->second);
        }
    }

    // Determine active columns (those with at least one non-empty value in data rows)
    std::vector<size_t> active_indices;
    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        int64_t src_idx = source_indices[c];
        if (src_idx < 0) {
            if (!drop_empty_columns) active_indices.push_back(c);
            continue;
        }

        if (!drop_empty_columns || data_rows.empty()) {
            active_indices.push_back(c);
            continue;
        }

        bool has_data = false;
        for (const auto& r : data_rows) {
            if (static_cast<size_t>(src_idx) < r.size()) {
                if (!CSVReader::trim(r[src_idx]).empty()) {
                    has_data = true;
                    break;
                }
            }
        }

        if (has_data) {
            active_indices.push_back(c);
        }
    }

    // Build final output headers
    std::vector<std::string> final_headers;
    for (size_t idx : active_indices) {
        final_headers.push_back(config_.target_headers[idx]);
    }

    if (stats) {
        stats->total_target_columns = config_.target_headers.size();
        stats->mapped_columns.clear();
        stats->unmapped_target_columns.clear();
        stats->rows_processed = data_rows.size();

        for (size_t c = 0; c < config_.target_headers.size(); ++c) {
            if (source_indices[c] >= 0) {
                stats->mapped_columns.push_back(config_.target_headers[c]);
            } else {
                stats->unmapped_target_columns.push_back(config_.target_headers[c]);
            }
        }
        stats->columns_mapped = stats->mapped_columns.size();
    }

    // Ensure output directory exists
    if (output_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
    }

    CSVWriter writer(output_path);
    if (!writer.is_open()) {
        return false;
    }

    // Write Header Row
    if (!writer.write_row(final_headers)) {
        return false;
    }

    // Stream through data rows
    std::vector<std::string> out_row;
    out_row.resize(active_indices.size());

    for (const auto& in_row : data_rows) {
        for (size_t i = 0; i < active_indices.size(); ++i) {
            size_t c = active_indices[i];
            int64_t src_idx = source_indices[c];
            if (src_idx >= 0 && static_cast<size_t>(src_idx) < in_row.size()) {
                out_row[i] = in_row[static_cast<size_t>(src_idx)];
            } else {
                out_row[i].clear();
            }
        }

        if (!writer.write_row(out_row)) {
            return false;
        }
    }

    writer.close();
    return true;
}

} // namespace sl
