#pragma once

#include "config_manager.hpp"
#include "csv_engine.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace sl {

struct TransformStats {
    size_t rows_processed = 0;
    size_t columns_mapped = 0;
    size_t total_target_columns = 0;
    std::vector<std::string> mapped_columns;
    std::vector<std::string> unmapped_target_columns;
};

class SchemaTransformer {
public:
    explicit SchemaTransformer(SchemaConfig config = SchemaConfig::get_default());

    void set_config(const SchemaConfig& config);
    const SchemaConfig& get_config() const;

    // Transform in-memory rows
    std::vector<std::vector<std::string>> transform_rows(
        const std::vector<std::vector<std::string>>& input_rows,
        TransformStats* stats = nullptr,
        bool drop_empty_columns = true) const;

    // Transform file to file
    bool transform_file(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_path,
        TransformStats* stats = nullptr,
        bool drop_empty_columns = true) const;

    // Build column map from raw input headers
    std::unordered_map<std::string, size_t> build_column_map(
        const std::vector<std::string>& source_headers) const;

private:
    SchemaConfig config_;
    // Fast normalized alias lookup: lowercase_alias -> lowercase_target_header
    std::unordered_map<std::string, std::string> alias_lookup_;

    void build_alias_lookup();
};

} // namespace sl
