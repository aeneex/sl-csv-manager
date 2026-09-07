#include "schema_transformer.hpp"
#include <random>
#include <unordered_set>


namespace {
std::string generate_random_36_id(std::unordered_set<std::string>& used_ids) {
    static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz-";
    static const size_t charset_size = sizeof(charset) - 1;
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, charset_size - 1);

    std::string result;
    result.resize(36);
    while (true) {
        for (size_t i = 0; i < 36; ++i) {
            result[i] = charset[dist(rng)];
        }
        if (used_ids.insert(result).second) {
            return result;
        }
    }
}
} // namespace

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
    bool drop_empty_columns,
    bool concat_name,
    bool indexify) const {

    std::vector<std::vector<std::string>> output;
    if (input_rows.empty()) {
        return output;
    }

    const auto& source_headers = input_rows[0];
    auto col_map = build_column_map(source_headers);

    // Precompute source indices for each target header
    std::vector<int64_t> source_indices(config_.target_headers.size(), -1);
    int64_t target_idx_first = -1;
    int64_t target_idx_last = -1;
    int64_t target_idx_name = -1;
    int64_t target_idx_id = -1;
    int64_t target_idx_index = -1;

    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        std::string clean_target = CSVReader::to_lower(CSVReader::trim(config_.target_headers[c]));
        if (clean_target == "first_name") target_idx_first = static_cast<int64_t>(c);
        else if (clean_target == "last_name") target_idx_last = static_cast<int64_t>(c);
        else if (clean_target == "name") target_idx_name = static_cast<int64_t>(c);
        else if (clean_target == "id") target_idx_id = static_cast<int64_t>(c);
        else if (clean_target == "index") target_idx_index = static_cast<int64_t>(c);

        auto it = col_map.find(clean_target);
        if (it != col_map.end()) {
            source_indices[c] = static_cast<int64_t>(it->second);
        }
    }

    int64_t src_first_idx = (target_idx_first >= 0) ? source_indices[target_idx_first] : -1;
    int64_t src_last_idx = (target_idx_last >= 0) ? source_indices[target_idx_last] : -1;

    // Determine the last row index containing a non-empty first_name cell
    int64_t last_first_name_row = -1;
    if (src_first_idx >= 0) {
        for (int64_t r = static_cast<int64_t>(input_rows.size()) - 1; r >= 1; --r) {
            if (static_cast<size_t>(src_first_idx) < input_rows[static_cast<size_t>(r)].size()) {
                if (!CSVReader::trim(input_rows[static_cast<size_t>(r)][static_cast<size_t>(src_first_idx)]).empty()) {
                    last_first_name_row = r;
                    break;
                }
            }
        }
    }

    // Helper lambda to compute concatenated name for a row
    auto get_concat_name_value = [&](const std::vector<std::string>& row) -> std::string {
        std::string fname, lname;
        if (src_first_idx >= 0 && static_cast<size_t>(src_first_idx) < row.size()) {
            fname = CSVReader::trim(row[static_cast<size_t>(src_first_idx)]);
        }
        if (src_last_idx >= 0 && static_cast<size_t>(src_last_idx) < row.size()) {
            lname = CSVReader::trim(row[static_cast<size_t>(src_last_idx)]);
        }
        if (!fname.empty() && !lname.empty()) {
            return fname + " " + lname;
        } else if (!fname.empty()) {
            return fname;
        } else if (!lname.empty()) {
            return lname;
        }
        int64_t src_name_idx = (target_idx_name >= 0) ? source_indices[target_idx_name] : -1;
        if (src_name_idx >= 0 && static_cast<size_t>(src_name_idx) < row.size()) {
            return row[static_cast<size_t>(src_name_idx)];
        }
        return "";
    };

    // Determine which target columns have at least one non-empty value
    std::vector<size_t> active_indices;
    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        int64_t src_idx = source_indices[c];
        bool is_name_col = (concat_name && static_cast<int64_t>(c) == target_idx_name);
        bool is_indexify_col = (indexify && last_first_name_row >= 1 &&
                               (static_cast<int64_t>(c) == target_idx_id || static_cast<int64_t>(c) == target_idx_index));

        if (src_idx < 0 && !is_name_col && !is_indexify_col) {
            if (!drop_empty_columns) active_indices.push_back(c);
            continue;
        }

        if (!drop_empty_columns || input_rows.size() <= 1 || is_indexify_col) {
            active_indices.push_back(c);
            continue;
        }

        bool has_data = false;
        for (size_t r = 1; r < input_rows.size(); ++r) {
            if (is_name_col) {
                if (!get_concat_name_value(input_rows[r]).empty()) {
                    has_data = true;
                    break;
                }
            } else if (static_cast<size_t>(src_idx) < input_rows[r].size()) {
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
            bool is_mapped = (source_indices[c] >= 0);
            if (concat_name && static_cast<int64_t>(c) == target_idx_name) {
                if (src_first_idx >= 0 || src_last_idx >= 0 || source_indices[c] >= 0) {
                    is_mapped = true;
                }
            }
            if (indexify && last_first_name_row >= 1 &&
                (static_cast<int64_t>(c) == target_idx_id || static_cast<int64_t>(c) == target_idx_index)) {
                is_mapped = true;
            }

            if (is_mapped) {
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
    std::unordered_set<std::string> used_ids;

    for (size_t r = 1; r < input_rows.size(); ++r) {
        const auto& in_row = input_rows[r];
        std::vector<std::string> out_row;
        out_row.reserve(active_indices.size());

        for (size_t idx : active_indices) {
            int64_t target_col_id = static_cast<int64_t>(idx);

            if (indexify && last_first_name_row >= 1 && static_cast<int64_t>(r) <= last_first_name_row &&
                (target_col_id == target_idx_id || target_col_id == target_idx_index)) {
                out_row.push_back(generate_random_36_id(used_ids));
            } else if (concat_name && target_col_id == target_idx_name) {
                out_row.push_back(get_concat_name_value(in_row));
            } else {
                int64_t src_idx = source_indices[idx];
                if (src_idx >= 0 && static_cast<size_t>(src_idx) < in_row.size()) {
                    out_row.push_back(in_row[static_cast<size_t>(src_idx)]);
                } else {
                    out_row.push_back("");
                }
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
    bool drop_empty_columns,
    bool concat_name,
    bool indexify) const {

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
    int64_t target_idx_first = -1;
    int64_t target_idx_last = -1;
    int64_t target_idx_name = -1;
    int64_t target_idx_id = -1;
    int64_t target_idx_index = -1;

    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        std::string clean_target = CSVReader::to_lower(CSVReader::trim(config_.target_headers[c]));
        if (clean_target == "first_name") target_idx_first = static_cast<int64_t>(c);
        else if (clean_target == "last_name") target_idx_last = static_cast<int64_t>(c);
        else if (clean_target == "name") target_idx_name = static_cast<int64_t>(c);
        else if (clean_target == "id") target_idx_id = static_cast<int64_t>(c);
        else if (clean_target == "index") target_idx_index = static_cast<int64_t>(c);

        auto it = col_map.find(clean_target);
        if (it != col_map.end()) {
            source_indices[c] = static_cast<int64_t>(it->second);
        }
    }

    int64_t src_first_idx = (target_idx_first >= 0) ? source_indices[target_idx_first] : -1;
    int64_t src_last_idx = (target_idx_last >= 0) ? source_indices[target_idx_last] : -1;

    // Determine the last row index containing a non-empty first_name cell (0-indexed in data_rows)
    int64_t last_first_name_row = -1;
    if (src_first_idx >= 0) {
        for (int64_t r = static_cast<int64_t>(data_rows.size()) - 1; r >= 0; --r) {
            if (static_cast<size_t>(src_first_idx) < data_rows[static_cast<size_t>(r)].size()) {
                if (!CSVReader::trim(data_rows[static_cast<size_t>(r)][static_cast<size_t>(src_first_idx)]).empty()) {
                    last_first_name_row = r;
                    break;
                }
            }
        }
    }

    auto get_concat_name_value = [&](const std::vector<std::string>& row) -> std::string {
        std::string fname, lname;
        if (src_first_idx >= 0 && static_cast<size_t>(src_first_idx) < row.size()) {
            fname = CSVReader::trim(row[static_cast<size_t>(src_first_idx)]);
        }
        if (src_last_idx >= 0 && static_cast<size_t>(src_last_idx) < row.size()) {
            lname = CSVReader::trim(row[static_cast<size_t>(src_last_idx)]);
        }
        if (!fname.empty() && !lname.empty()) {
            return fname + " " + lname;
        } else if (!fname.empty()) {
            return fname;
        } else if (!lname.empty()) {
            return lname;
        }
        int64_t src_name_idx = (target_idx_name >= 0) ? source_indices[target_idx_name] : -1;
        if (src_name_idx >= 0 && static_cast<size_t>(src_name_idx) < row.size()) {
            return row[static_cast<size_t>(src_name_idx)];
        }
        return "";
    };

    // Determine active columns (those with at least one non-empty value in data rows)
    std::vector<size_t> active_indices;
    for (size_t c = 0; c < config_.target_headers.size(); ++c) {
        int64_t src_idx = source_indices[c];
        bool is_name_col = (concat_name && static_cast<int64_t>(c) == target_idx_name);
        bool is_indexify_col = (indexify && last_first_name_row >= 0 &&
                               (static_cast<int64_t>(c) == target_idx_id || static_cast<int64_t>(c) == target_idx_index));

        if (src_idx < 0 && !is_name_col && !is_indexify_col) {
            if (!drop_empty_columns) active_indices.push_back(c);
            continue;
        }

        if (!drop_empty_columns || data_rows.empty() || is_indexify_col) {
            active_indices.push_back(c);
            continue;
        }

        bool has_data = false;
        for (const auto& r : data_rows) {
            if (is_name_col) {
                if (!get_concat_name_value(r).empty()) {
                    has_data = true;
                    break;
                }
            } else if (static_cast<size_t>(src_idx) < r.size()) {
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
            bool is_mapped = (source_indices[c] >= 0);
            if (concat_name && static_cast<int64_t>(c) == target_idx_name) {
                if (src_first_idx >= 0 || src_last_idx >= 0 || source_indices[c] >= 0) {
                    is_mapped = true;
                }
            }
            if (indexify && last_first_name_row >= 0 &&
                (static_cast<int64_t>(c) == target_idx_id || static_cast<int64_t>(c) == target_idx_index)) {
                is_mapped = true;
            }

            if (is_mapped) {
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
    std::unordered_set<std::string> used_ids;
    std::vector<std::string> out_row;
    out_row.resize(active_indices.size());

    for (size_t r = 0; r < data_rows.size(); ++r) {
        const auto& in_row = data_rows[r];

        for (size_t i = 0; i < active_indices.size(); ++i) {
            size_t c = active_indices[i];
            int64_t target_col_id = static_cast<int64_t>(c);

            if (indexify && last_first_name_row >= 0 && static_cast<int64_t>(r) <= last_first_name_row &&
                (target_col_id == target_idx_id || target_col_id == target_idx_index)) {
                out_row[i] = generate_random_36_id(used_ids);
            } else if (concat_name && target_col_id == target_idx_name) {
                out_row[i] = get_concat_name_value(in_row);
            } else {
                int64_t src_idx = source_indices[c];
                if (src_idx >= 0 && static_cast<size_t>(src_idx) < in_row.size()) {
                    out_row[i] = in_row[static_cast<size_t>(src_idx)];
                } else {
                    out_row[i].clear();
                }
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
