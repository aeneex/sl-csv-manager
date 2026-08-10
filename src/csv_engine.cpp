#include "csv_engine.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace sl {

CSVReader::CSVReader(const std::filesystem::path& filepath)
    : file_stream_(filepath, std::ios::binary), in_stream_(file_stream_) {
}

CSVReader::CSVReader(std::istream& stream)
    : in_stream_(stream) {
}

bool CSVReader::is_open() const {
    if (file_stream_.is_open()) {
        return file_stream_.good();
    }
    return in_stream_.good();
}

void CSVReader::close() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void CSVReader::check_bom() {
    if (has_bom_checked_) return;
    has_bom_checked_ = true;

    if (!in_stream_.good()) return;

    // Check for UTF-8 BOM: 0xEF, 0xBB, 0xBF
    char b1 = 0, b2 = 0, b3 = 0;
    if (in_stream_.get(b1)) {
        if (static_cast<unsigned char>(b1) == 0xEF) {
            if (in_stream_.get(b2) && static_cast<unsigned char>(b2) == 0xBB) {
                if (in_stream_.get(b3) && static_cast<unsigned char>(b3) == 0xBF) {
                    // BOM detected and consumed
                    return;
                } else {
                    if (in_stream_.good()) in_stream_.unget();
                }
            } else {
                if (in_stream_.good()) in_stream_.unget();
            }
        }
        in_stream_.unget();
    }
}

bool CSVReader::read_row(std::vector<std::string>& row) {
    row.clear();
    check_bom();

    if (!in_stream_.good() && in_stream_.eof()) {
        return false;
    }

    std::string field;
    bool in_quotes = false;
    bool row_has_content = false;
    char c = 0;

    while (in_stream_.get(c)) {
        row_has_content = true;

        if (in_quotes) {
            if (c == '"') {
                if (in_stream_.peek() == '"') {
                    // Escaped quote: ""
                    in_stream_.get(c);
                    field += '"';
                } else {
                    // End of quoted section
                    in_quotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                row.push_back(field);
                field.clear();
            } else if (c == '\r') {
                if (in_stream_.peek() == '\n') {
                    in_stream_.get(); // consume \n
                }
                row.push_back(field);
                return true;
            } else if (c == '\n') {
                row.push_back(field);
                return true;
            } else {
                field += c;
            }
        }
    }

    if (row_has_content || !field.empty()) {
        row.push_back(field);
        return true;
    }

    return false;
}

std::vector<std::vector<std::string>> CSVReader::read_all() {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    while (read_row(row)) {
        // Skip completely empty lines if at EOF
        if (row.size() == 1 && row[0].empty() && in_stream_.eof()) {
            break;
        }
        rows.push_back(row);
    }
    return rows;
}

std::string CSVReader::trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && (std::isspace(static_cast<unsigned char>(*start)) || *start == '\0')) {
        ++start;
    }
    auto end = str.end();
    do {
        --end;
    } while (std::distance(start, end) > 0 && (std::isspace(static_cast<unsigned char>(*end)) || *end == '\0'));

    return (start <= end) ? std::string(start, end + 1) : std::string();
}

std::string CSVReader::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

CSVWriter::CSVWriter(const std::filesystem::path& filepath)
    : file_stream_(filepath, std::ios::binary), out_stream_(file_stream_) {
}

CSVWriter::CSVWriter(std::ostream& stream)
    : out_stream_(stream) {
}

CSVWriter::~CSVWriter() {
    flush();
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

bool CSVWriter::is_open() const {
    if (file_stream_.is_open()) {
        return file_stream_.good();
    }
    return out_stream_.good();
}

std::string CSVWriter::escape_field(const std::string& field) {
    bool needs_quotes = false;
    for (char ch : field) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return field;
    }

    std::string result = "\"";
    for (char ch : field) {
        if (ch == '"') {
            result += "\"\"";
        } else {
            result += ch;
        }
    }
    result += "\"";
    return result;
}

bool CSVWriter::write_row(const std::vector<std::string>& row) {
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) {
            out_stream_ << ",";
        }
        out_stream_ << escape_field(row[i]);
    }
    out_stream_ << "\r\n";
    return out_stream_.good();
}

bool CSVWriter::write_all(const std::vector<std::vector<std::string>>& rows) {
    for (const auto& row : rows) {
        if (!write_row(row)) {
            return false;
        }
    }
    return true;
}

void CSVWriter::flush() {
    out_stream_.flush();
}

void CSVWriter::close() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

} // namespace sl
