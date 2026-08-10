#include "config_manager.hpp"
#include "csv_engine.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

namespace sl {

SchemaConfig SchemaConfig::get_default() {
    SchemaConfig config;
    config.target_headers = {
        "id", "index", "first_name", "last_name", "personal_email", "email", "email_status", "valid_mobile_number", 
        "name", "headline", "title", "linkedin_url", "person_linkedin_profile_summary", "skills", "department", "sub_departments", "functions", 
        "seniority", "country", "state", "city", "organization_name", "organization_linkedin_url", "organization_linkedin_description", "organization_overview",
        "organization_founded_year", "estimated_num_employees", "organization_primary_domain", 
        "organization_country", "organization_state", "organization_city", "organization_postal_code", 
        "raw_address", "industry", "keywords"
    };

    config.mappings = {
        {"id", {"_id"}},
        {"index", {"_index"}},
        {"first_name", {"First Name", "firstName"}},
        {"last_name", {"Last Name", "lastName"}},
        {"email", {"Email Business", "email"}},
        {"email_status", {"Domain Settings", "Email Status", "emailStatus"}},
        {"valid_mobile_number", {"cellphone"}},
        {"name", {"Full Name", "name"}},
        {"headline", {"Headline", "headline"}},
        {"title", {"Title", "title"}},
        {"linkedin_url", {"LinkedIn", "Person Linkedin Url", "linkedinUrl"}},
        {"department", {"Department", "department"}},
        {"seniority", {"Seniority", "seniority"}},
        {"country", {"Country", "country"}},
        {"state", {"State", "address.state"}},
        {"city", {"City", "city"}},
        {"organization_name", {"Org", "Company Name", "organization.name"}},
        {"organization_linkedin_url", {"First Current Company LinkedIn", "Company LinkedIn", "Company Linkedin Url", "organization.linkedinUrl"}},
        {"organization_linkedin_description", {"organization.about"}},
        {"organization_founded_year", {"First Current Company Founding Year", "Company Founding Year", "organization.foundedYear"}},
        {"estimated_num_employees", {"First Current Company Size", "Company Size", "organization.employeesOnLinkedin"}},
        {"organization_primary_domain", {"First Current Company Website", "Company Website", "Website", "organization.domain"}},
        {"raw_address", {"Company Address"}},
        {"organization_country", {"Company Country", "jobLocation.country"}},
        {"organization_state", {"Company State", "organization.state"}},
        {"organization_city", {"Company City", "jobLocation.city"}},
        {"organization_postal_code", {"Company Postal Code", "organization.postcode"}},
        {"industry", {"First Current Company Industry", "Company Industry", "organization.industry"}},
        {"keywords", {"First Current Company Product and Services", "Company Product and Services", "organization.naicsDescriptions"}}
    };

    return config;
}

std::string ConfigManager::serialize_json(const SchemaConfig& config) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"target_headers\": [\n";
    for (size_t i = 0; i < config.target_headers.size(); ++i) {
        ss << "    \"" << config.target_headers[i] << "\"";
        if (i + 1 < config.target_headers.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"mappings\": {\n";
    size_t map_idx = 0;
    for (const auto& [target, aliases] : config.mappings) {
        ss << "    \"" << target << "\": [";
        for (size_t a = 0; a < aliases.size(); ++a) {
            ss << "\"" << aliases[a] << "\"";
            if (a + 1 < aliases.size()) ss << ", ";
        }
        ss << "]";
        if (++map_idx < config.mappings.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

namespace {
    void skip_whitespace_and_comments(const std::string& s, size_t& pos) {
        while (pos < s.size()) {
            if (std::isspace(static_cast<unsigned char>(s[pos]))) {
                pos++;
            } else if (pos + 1 < s.size() && s[pos] == '/' && s[pos + 1] == '/') {
                pos += 2;
                while (pos < s.size() && s[pos] != '\n') pos++;
            } else {
                break;
            }
        }
    }

    bool parse_json_string(const std::string& s, size_t& pos, std::string& out) {
        skip_whitespace_and_comments(s, pos);
        if (pos >= s.size() || s[pos] != '"') return false;
        pos++; // skip opening quote
        out.clear();
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') {
                return true;
            } else if (c == '\\' && pos < s.size()) {
                char next = s[pos++];
                switch (next) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default: out += next; break;
                }
            } else {
                out += c;
            }
        }
        return false;
    }

    bool parse_string_array(const std::string& s, size_t& pos, std::vector<std::string>& list) {
        skip_whitespace_and_comments(s, pos);
        if (pos >= s.size() || s[pos] != '[') return false;
        pos++; // skip '['
        list.clear();

        while (pos < s.size()) {
            skip_whitespace_and_comments(s, pos);
            if (pos < s.size() && s[pos] == ']') {
                pos++;
                return true;
            }

            std::string item;
            if (!parse_json_string(s, pos, item)) {
                return false;
            }
            list.push_back(item);

            skip_whitespace_and_comments(s, pos);
            if (pos < s.size() && s[pos] == ',') {
                pos++;
            } else if (pos < s.size() && s[pos] == ']') {
                pos++;
                return true;
            } else {
                return false;
            }
        }
        return false;
    }
}

bool ConfigManager::parse_json(const std::string& json_str, SchemaConfig& config) {
    size_t pos = 0;
    skip_whitespace_and_comments(json_str, pos);
    if (pos >= json_str.size() || json_str[pos] != '{') return false;
    pos++;

    SchemaConfig parsed;
    bool found_target_headers = false;

    while (pos < json_str.size()) {
        skip_whitespace_and_comments(json_str, pos);
        if (pos < json_str.size() && json_str[pos] == '}') {
            pos++;
            break;
        }

        std::string key;
        if (!parse_json_string(json_str, pos, key)) return false;

        skip_whitespace_and_comments(json_str, pos);
        if (pos >= json_str.size() || json_str[pos] != ':') return false;
        pos++; // skip ':'

        skip_whitespace_and_comments(json_str, pos);
        if (key == "target_headers") {
            if (!parse_string_array(json_str, pos, parsed.target_headers)) return false;
            found_target_headers = true;
        } else if (key == "mappings") {
            if (pos >= json_str.size() || json_str[pos] != '{') return false;
            pos++; // skip '{'

            while (pos < json_str.size()) {
                skip_whitespace_and_comments(json_str, pos);
                if (pos < json_str.size() && json_str[pos] == '}') {
                    pos++;
                    break;
                }

                std::string target_col;
                if (!parse_json_string(json_str, pos, target_col)) return false;

                skip_whitespace_and_comments(json_str, pos);
                if (pos >= json_str.size() || json_str[pos] != ':') return false;
                pos++;

                std::vector<std::string> aliases;
                if (!parse_string_array(json_str, pos, aliases)) return false;

                parsed.mappings[target_col] = aliases;

                skip_whitespace_and_comments(json_str, pos);
                if (pos < json_str.size() && json_str[pos] == ',') {
                    pos++;
                } else if (pos < json_str.size() && json_str[pos] == '}') {
                    pos++;
                    break;
                }
            }
        } else {
            // Unknown key, skip value
            return false;
        }

        skip_whitespace_and_comments(json_str, pos);
        if (pos < json_str.size() && json_str[pos] == ',') {
            pos++;
        }
    }

    if (found_target_headers && !parsed.target_headers.empty()) {
        config = parsed;
        return true;
    }
    return false;
}

SchemaConfig ConfigManager::load_or_create_default(const std::filesystem::path& config_path) {
    SchemaConfig config;
    if (std::filesystem::exists(config_path)) {
        if (load_from_file(config, config_path)) {
            return config;
        }
        std::cerr << "[Warning] Failed to parse config file at: " << config_path << ". Using built-in defaults.\n";
    }

    config = SchemaConfig::get_default();
    save_to_file(config, config_path);
    return config;
}

bool ConfigManager::save_to_file(const SchemaConfig& config, const std::filesystem::path& config_path) {
    try {
        std::ofstream out(config_path);
        if (!out.is_open()) return false;
        out << serialize_json(config);
        return out.good();
    } catch (...) {
        return false;
    }
}

bool ConfigManager::load_from_file(SchemaConfig& config, const std::filesystem::path& config_path) {
    try {
        std::ifstream in(config_path);
        if (!in.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return parse_json(content, config);
    } catch (...) {
        return false;
    }
}

} // namespace sl
