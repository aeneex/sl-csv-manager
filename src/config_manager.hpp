#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace sl {

struct SchemaConfig {
    std::vector<std::string> target_headers;
    std::map<std::string, std::vector<std::string>> mappings;

    static SchemaConfig get_default();
};

class ConfigManager {
public:
    static SchemaConfig load_or_create_default(const std::filesystem::path& config_path);
    static bool save_to_file(const SchemaConfig& config, const std::filesystem::path& config_path);
    static bool load_from_file(SchemaConfig& config, const std::filesystem::path& config_path);

private:
    static std::string serialize_json(const SchemaConfig& config);
    static bool parse_json(const std::string& json_str, SchemaConfig& config);
};

} // namespace sl
