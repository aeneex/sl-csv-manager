#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace sl {

struct SplitResult {
    bool success = false;
    size_t total_data_rows = 0;
    size_t parts_created = 0;
    std::vector<std::filesystem::path> output_files;
    std::filesystem::path moved_original_path;
    std::string error_message;
};

class CSVSplitter {
public:
    // Split into N equal parts (default 2)
    static SplitResult split_into_parts(
        const std::filesystem::path& input_path,
        size_t num_parts = 2,
        bool keep_header_in_all = true,
        bool move_original = false,
        const std::string& backup_folder_name = "original files");

    // Split by max data rows per file chunk
    static SplitResult split_by_max_rows(
        const std::filesystem::path& input_path,
        size_t max_rows_per_file,
        bool keep_header_in_all = true,
        bool move_original = false,
        const std::string& backup_folder_name = "original files");

    // Safe move file to backup folder
    static bool move_to_backup(
        const std::filesystem::path& file_path,
        const std::string& backup_folder_name,
        std::filesystem::path& final_backup_path);
};

} // namespace sl
