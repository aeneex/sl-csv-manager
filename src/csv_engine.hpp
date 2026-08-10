#pragma once

#include <string>
#include <vector>
#include <istream>
#include <ostream>
#include <fstream>
#include <filesystem>

namespace sl {

class CSVReader {
public:
    explicit CSVReader(const std::filesystem::path& filepath);
    explicit CSVReader(std::istream& stream);
    ~CSVReader() = default;

    bool is_open() const;
    void close();
    bool read_row(std::vector<std::string>& row);
    std::vector<std::vector<std::string>> read_all();

    static std::vector<std::string> parse_line(const std::string& line);
    static std::string trim(const std::string& str);
    static std::string to_lower(const std::string& str);

private:
    std::ifstream file_stream_;
    std::istream& in_stream_;
    bool has_bom_checked_ = false;

    void check_bom();
};

class CSVWriter {
public:
    explicit CSVWriter(const std::filesystem::path& filepath);
    explicit CSVWriter(std::ostream& stream);
    ~CSVWriter();

    bool is_open() const;
    bool write_row(const std::vector<std::string>& row);
    bool write_all(const std::vector<std::vector<std::string>>& rows);
    void flush();
    void close();

    static std::string escape_field(const std::string& field);

private:
    std::ofstream file_stream_;
    std::ostream& out_stream_;
};

} // namespace sl
