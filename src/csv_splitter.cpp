#include "csv_splitter.hpp"
#include "csv_engine.hpp"
#include <iostream>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace sl {

bool CSVSplitter::move_to_backup(
    const std::filesystem::path& file_path,
    const std::string& backup_folder_name,
    std::filesystem::path& final_backup_path) {
    
    try {
        if (!std::filesystem::exists(file_path)) {
            return false;
        }

        std::filesystem::path parent = file_path.has_parent_path() ? file_path.parent_path() : std::filesystem::current_path();
        std::filesystem::path backup_dir = parent / backup_folder_name;

        if (!std::filesystem::exists(backup_dir)) {
            std::filesystem::create_directories(backup_dir);
        }

        std::filesystem::path target_file = backup_dir / file_path.filename();
        
        // If file already exists in backup, append timestamp
        if (std::filesystem::exists(target_file)) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
            localtime_s(&tm_buf, &time);
#else
            localtime_r(&time, &tm_buf);
#endif
            std::ostringstream ss;
            ss << file_path.stem().string() << "_"
               << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
               << file_path.extension().string();
            target_file = backup_dir / ss.str();
        }

        std::error_code ec;
        std::filesystem::rename(file_path, target_file, ec);
        if (ec) {
            // Fallback to copy + remove
            std::filesystem::copy_file(file_path, target_file, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec) {
                std::filesystem::remove(file_path, ec);
            }
        }

        if (!ec) {
            final_backup_path = target_file;
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

SplitResult CSVSplitter::split_into_parts(
    const std::filesystem::path& input_path,
    size_t num_parts,
    bool keep_header_in_all,
    bool move_original,
    const std::string& backup_folder_name) {

    SplitResult res;
    if (num_parts == 0) num_parts = 1;

    std::vector<std::string> header;
    std::vector<std::vector<std::string>> data_rows;

    {
        CSVReader reader(input_path);
        if (!reader.is_open()) {
            res.error_message = "Could not open file: " + input_path.string();
            return res;
        }

        if (!reader.read_row(header)) {
            res.error_message = "File is empty: " + input_path.string();
            return res;
        }

        std::vector<std::string> row;
        while (reader.read_row(row)) {
            if (row.size() == 1 && row[0].empty()) continue;
            data_rows.push_back(std::move(row));
        }
    }

    res.total_data_rows = data_rows.size();

    if (data_rows.empty()) {
        res.error_message = "No data rows found in file: " + input_path.string();
        return res;
    }

    size_t chunk_size = static_cast<size_t>(std::ceil(static_cast<double>(data_rows.size()) / static_cast<double>(num_parts)));
    if (chunk_size == 0) chunk_size = 1;

    std::filesystem::path parent = input_path.has_parent_path() ? input_path.parent_path() : std::filesystem::current_path();
    std::filesystem::path split_done_dir = parent / "split done";
    std::error_code ec;
    if (!std::filesystem::exists(split_done_dir)) {
        std::filesystem::create_directories(split_done_dir, ec);
    }

    std::string stem = input_path.stem().string();
    std::string ext = input_path.extension().string();
    if (ext.empty()) ext = ".csv";

    for (size_t p = 0; p < num_parts; ++p) {
        size_t start_idx = p * chunk_size;
        if (start_idx >= data_rows.size()) break;
        size_t end_idx = std::min(start_idx + chunk_size, data_rows.size());

        std::ostringstream out_filename;
        out_filename << stem << " - " << (p + 1) << ext;
        std::filesystem::path out_path = split_done_dir / out_filename.str();

        CSVWriter writer(out_path);
        if (!writer.is_open()) {
            res.error_message = "Failed to write part file: " + out_path.string();
            return res;
        }

        if (keep_header_in_all || p == 0) {
            writer.write_row(header);
        }

        for (size_t i = start_idx; i < end_idx; ++i) {
            writer.write_row(data_rows[i]);
        }
        writer.close();

        res.output_files.push_back(out_path);
        res.parts_created++;
    }

    if (move_original) {
        std::filesystem::path backed_up_path;
        if (move_to_backup(input_path, backup_folder_name, backed_up_path)) {
            res.moved_original_path = backed_up_path;
        }
    }

    res.success = true;
    return res;
}

SplitResult CSVSplitter::split_by_max_rows(
    const std::filesystem::path& input_path,
    size_t max_rows_per_file,
    bool keep_header_in_all,
    bool move_original,
    const std::string& backup_folder_name) {

    SplitResult res;
    if (max_rows_per_file == 0) max_rows_per_file = 1000;

    CSVReader reader(input_path);
    if (!reader.is_open()) {
        res.error_message = "Could not open file: " + input_path.string();
        return res;
    }

    std::vector<std::string> header;
    if (!reader.read_row(header)) {
        res.error_message = "File is empty: " + input_path.string();
        return res;
    }

    std::filesystem::path parent = input_path.has_parent_path() ? input_path.parent_path() : std::filesystem::current_path();
    std::filesystem::path split_done_dir = parent / "split done";
    std::error_code ec;
    if (!std::filesystem::exists(split_done_dir)) {
        std::filesystem::create_directories(split_done_dir, ec);
    }

    std::string stem = input_path.stem().string();
    std::string ext = input_path.extension().string();
    if (ext.empty()) ext = ".csv";

    size_t current_chunk = 1;
    size_t rows_in_chunk = 0;
    std::unique_ptr<CSVWriter> current_writer;

    auto open_new_chunk = [&]() -> bool {
        std::ostringstream out_filename;
        out_filename << stem << " - " << current_chunk << ext;
        std::filesystem::path out_path = split_done_dir / out_filename.str();
        current_writer = std::make_unique<CSVWriter>(out_path);
        if (!current_writer->is_open()) {
            return false;
        }
        if (keep_header_in_all || current_chunk == 1) {
            current_writer->write_row(header);
        }
        res.output_files.push_back(out_path);
        res.parts_created++;
        rows_in_chunk = 0;
        return true;
    };

    std::vector<std::string> row;
    while (reader.read_row(row)) {
        if (row.size() == 1 && row[0].empty()) continue;

        if (!current_writer || rows_in_chunk >= max_rows_per_file) {
            if (current_writer) {
                current_writer->close();
                current_chunk++;
            }
            if (!open_new_chunk()) {
                res.error_message = "Failed to open new chunk file.";
                return res;
            }
        }

        current_writer->write_row(row);
        rows_in_chunk++;
        res.total_data_rows++;
    }

    if (current_writer) {
        current_writer->close();
    }
    reader.close();

    if (res.total_data_rows == 0) {
        res.error_message = "No data rows found in file: " + input_path.string();
        return res;
    }

    if (move_original) {
        std::filesystem::path backed_up_path;
        if (move_to_backup(input_path, backup_folder_name, backed_up_path)) {
            res.moved_original_path = backed_up_path;
        }
    }

    res.success = true;
    return res;
}

} // namespace sl
