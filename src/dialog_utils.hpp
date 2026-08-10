#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace sl {

class DialogUtils {
public:
    // Opens a native Windows Explorer File Open dialog to pick a CSV file.
    // Returns empty path if user cancels.
    static std::filesystem::path select_csv_file(const std::string& title = "Select CSV File");

    // Opens a native Windows Explorer Folder selection dialog.
    // Returns empty path if user cancels.
    static std::filesystem::path select_folder(const std::string& title = "Select Folder");
};

} // namespace sl
