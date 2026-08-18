#include "csv_engine.hpp"
#include "config_manager.hpp"
#include "schema_transformer.hpp"
#include "csv_splitter.hpp"
#include "dialog_utils.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace fs = std::filesystem;

void enable_utf8_console() {
#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::string clean_input_path(std::string path_str) {
    path_str = sl::CSVReader::trim(path_str);
    if (path_str.size() >= 2 && path_str.front() == '"' && path_str.back() == '"') {
        path_str = path_str.substr(1, path_str.size() - 2);
    }
    if (path_str.size() >= 2 && path_str.front() == '\'' && path_str.back() == '\'') {
        path_str = path_str.substr(1, path_str.size() - 2);
    }
    return sl::CSVReader::trim(path_str);
}

bool prompt_yes_no_default_yes(const std::string& prompt_text) {
    std::cout << prompt_text;
    std::string ans;
    std::getline(std::cin, ans);
    ans = sl::CSVReader::to_lower(sl::CSVReader::trim(ans));
    if (ans.empty() || ans == "y" || ans == "yes") {
        return true;
    }
    return false;
}

void print_banner() {
    std::cout << "===============================================================\n";
    std::cout << "                             SLMAN                             \n";
    std::cout << "                 CSV Formatter & Splitter Tool                 \n";
    std::cout << "===============================================================\n\n";
}

void print_help() {
    print_banner();
    std::cout << "Usage:\n";
    std::cout << "  slman [command] [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  format <input.csv> [-o <output.csv>] [-c <config.json>] [--drop-empty]\n";
    std::cout << "      Formats raw CSV columns into the standard 35-column schema.\n";
    std::cout << "      (By default, creates all 35 schema columns with unmapped fields empty).\n";
    std::cout << "      Flags:\n";
    std::cout << "        --drop-empty    Delete/omit completely empty unmapped columns\n";
    std::cout << "        --all-columns   Generate all schema columns (default)\n";
    std::cout << "      (Alias: transform)\n\n";
    std::cout << "  split <input.csv> [--max-rows <R>] [--parts <N>] [--no-headers] [--backup]\n";
    std::cout << "      Splits CSV by max rows per file or into N parts and saves to 'split done/'.\n";
    std::cout << "      Flags:\n";
    std::cout << "        --max-rows <R>  Maximum data rows per file chunk (e.g. 1000)\n";
    std::cout << "        --parts <N>     Number of parts to split into (default 2 if not using --max-rows)\n";
    std::cout << "        --no-headers    Do not repeat header in parts 2..N (only part 1 has header)\n";
    std::cout << "        --backup        Move original file to 'original files/'\n\n";
    std::cout << "  bulk [dir_path] [--format] [--max-rows <R>] [--split <N>] [--no-headers] [--drop-empty]\n";
    std::cout << "      Batch processes all CSV files in a directory.\n";
    std::cout << "      Flags:\n";
    std::cout << "        --format        Format CSV files into technical schema (default action)\n";
    std::cout << "        --max-rows <R>  Split all CSVs into chunks of maximum R rows (e.g. 1000)\n";
    std::cout << "        --split <N>     Split all CSVs into N parts (default 2)\n";
    std::cout << "        --no-headers    Do not repeat header in subsequent parts\n";
    std::cout << "        --drop-empty    Delete/omit completely empty unmapped columns\n\n";
    std::cout << "  init-config\n";
    std::cout << "      Generates the default 'mapping_config.json' file.\n\n";
    std::cout << "  (no arguments)\n";
    std::cout << "      Starts the interactive console wizard.\n";
}

bool do_format_file(const fs::path& input_path, fs::path output_path, const sl::SchemaConfig& config, bool drop_empty = false) {
    if (output_path.empty()) {
        fs::path parent = input_path.has_parent_path() ? input_path.parent_path() : fs::current_path();
        fs::path format_done_dir = parent / "format done";
        std::error_code ec;
        if (!fs::exists(format_done_dir)) {
            fs::create_directories(format_done_dir, ec);
        }
        output_path = format_done_dir / input_path.filename();
    }

    std::cout << "--> Formatting: " << input_path.filename().string() << "...\n";
    sl::SchemaTransformer transformer(config);
    sl::TransformStats stats;

    if (!transformer.transform_file(input_path, output_path, &stats, drop_empty)) {
        std::cerr << " [ERROR] Failed to format file: " << input_path.string() << "\n";
        return false;
    }

    std::cout << " [SUCCESS] Formatted " << stats.rows_processed << " rows.\n";
    std::cout << "           Mapped columns: " << stats.columns_mapped << " / " << stats.total_target_columns << "\n";
    if (!drop_empty) {
        std::cout << "           Output schema:  All " << stats.total_target_columns << " columns created (unmapped kept empty)\n";
    } else {
        std::cout << "           Output schema:  " << stats.columns_mapped << " active columns (empty columns dropped)\n";
    }
    std::cout << "           Saved to: " << output_path.string() << "\n\n";
    return true;
}

bool do_split_file(const fs::path& input_path, size_t parts, size_t max_rows, bool keep_header_in_all, bool move_backup = false) {
    std::cout << "--> Splitting: " << input_path.filename().string() << "...\n";
    sl::SplitResult res;

    if (max_rows > 0) {
        res = sl::CSVSplitter::split_by_max_rows(input_path, max_rows, keep_header_in_all, move_backup);
    } else {
        if (parts == 0) parts = 2;
        res = sl::CSVSplitter::split_into_parts(input_path, parts, keep_header_in_all, move_backup);
    }

    if (!res.success) {
        std::cerr << " [ERROR] " << res.error_message << "\n";
        return false;
    }

    std::cout << " [SUCCESS] Split " << res.total_data_rows << " rows into " << res.parts_created << " files";
    if (keep_header_in_all) {
        std::cout << " (headers preserved in all parts):\n";
    } else {
        std::cout << " (header preserved in Part 1 only):\n";
    }

    for (const auto& f : res.output_files) {
        std::cout << "           * " << f.filename().string() << "\n";
    }
    if (move_backup && !res.moved_original_path.empty()) {
        std::cout << "           Original moved to: " << res.moved_original_path.string() << "\n";
    }
    std::cout << "\n";
    return true;
}

void do_bulk_dir(const fs::path& dir_path, bool format_mode, size_t split_parts, size_t split_max_rows, bool keep_header_in_all, const sl::SchemaConfig& config, bool drop_empty = false) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        std::cerr << " [ERROR] Directory does not exist: " << dir_path.string() << "\n";
        return;
    }

    std::vector<fs::path> csv_files;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && sl::CSVReader::to_lower(entry.path().extension().string()) == ".csv") {
            // Avoid touching files in original files, format done, split done, or done folders
            std::string parent_name = entry.path().parent_path().filename().string();
            if (parent_name == "original files" || parent_name == "format done" || parent_name == "split done" || parent_name == "done") continue;
            csv_files.push_back(entry.path());
        }
    }

    if (csv_files.empty()) {
        std::cout << "No CSV files found in " << dir_path.string() << "\n";
        return;
    }

    std::cout << "\nFound " << csv_files.size() << " CSV file(s) in " << dir_path.string() << "\n";
    std::cout << "---------------------------------------------------------------\n";

    size_t success_count = 0;
    size_t error_count = 0;

    for (const auto& csv : csv_files) {
        bool ok = false;
        if (format_mode) {
            ok = do_format_file(csv, "", config, drop_empty);
        } else if (split_max_rows > 0 || split_parts > 0) {
            ok = do_split_file(csv, split_parts, split_max_rows, keep_header_in_all, false);
        }

        if (ok) success_count++;
        else error_count++;
    }

    std::cout << "===============================================================\n";
    std::cout << "Bulk Execution Complete!\n";
    std::cout << "Successfully processed: " << success_count << " files\n";
    std::cout << "Errors/Skipped:        " << error_count << " files\n";
    std::cout << "===============================================================\n\n";
}

void interactive_menu(const sl::SchemaConfig& config) {
    while (true) {
        print_banner();
        std::cout << "Select an operation:\n\n";
        std::cout << "  [1] Format single CSV file to Technical Schema\n";
        std::cout << "  [2] Split single CSV file (by max rows or parts)\n";
        std::cout << "  [3] Bulk: Format all CSV files in a folder\n";
        std::cout << "  [4] Bulk: Split all CSV files in a folder (by max rows or parts)\n";
        std::cout << "  [5] View Target Schema & Mappings\n";
        std::cout << "  [6] Exit\n\n";
        std::cout << "Enter choice (1-6): ";

        std::string choice_str;
        if (!std::getline(std::cin, choice_str)) break;
        choice_str = sl::CSVReader::trim(choice_str);

        if (choice_str == "6" || choice_str == "q" || choice_str == "exit") {
            std::cout << "\nGoodbye!\n";
            break;
        }

        if (choice_str == "1") {
            std::cout << "\nOpening File Explorer to select CSV file...\n";
            fs::path p = sl::DialogUtils::select_csv_file("Select CSV File to Format");
            if (!p.empty() && fs::exists(p)) {
                std::cout << "Selected file: " << p.string() << "\n";
                bool create_all = prompt_yes_no_default_yes("Create all schema columns (keep unmapped empty)? (Y/n) [default Y]: ");
                do_format_file(p, "", config, !create_all);
            } else {
                std::cout << "No file selected (operation cancelled).\n\n";
            }
        } else if (choice_str == "2") {
            std::cout << "\nOpening File Explorer to select CSV file...\n";
            fs::path p = sl::DialogUtils::select_csv_file("Select CSV File to Split");
            if (!p.empty() && fs::exists(p)) {
                std::cout << "Selected file: " << p.string() << "\n\n";
                std::cout << "Choose split method:\n";
                std::cout << "  [1] By Maximum Rows per file (e.g. 1000 rows/file) [Default]\n";
                std::cout << "  [2] Into N equal parts (e.g. 2 parts)\n";
                std::cout << "Enter split method (1/2) [default 1]: ";
                std::string mode_str;
                std::getline(std::cin, mode_str);
                mode_str = sl::CSVReader::trim(mode_str);

                size_t parts = 0;
                size_t max_rows = 0;

                if (mode_str == "2") {
                    std::cout << "Enter number of parts [default 2]: ";
                    std::string parts_str;
                    std::getline(std::cin, parts_str);
                    parts = 2;
                    try {
                        if (!parts_str.empty()) parts = std::stoul(parts_str);
                    } catch (...) { parts = 2; }
                    if (parts == 0) parts = 2;
                } else {
                    std::cout << "Enter max data rows per file [default 1000]: ";
                    std::string rows_str;
                    std::getline(std::cin, rows_str);
                    max_rows = 1000;
                    try {
                        if (!rows_str.empty()) max_rows = std::stoul(rows_str);
                    } catch (...) { max_rows = 1000; }
                    if (max_rows == 0) max_rows = 1000;
                }

                bool keep_headers = prompt_yes_no_default_yes("Keep header in all split files? (Y/n) [default Y]: ");
                do_split_file(p, parts, max_rows, keep_headers, false);
            } else {
                std::cout << "No file selected (operation cancelled).\n\n";
            }
        } else if (choice_str == "3") {
            std::cout << "\nOpening File Explorer to select folder...\n";
            fs::path p = sl::DialogUtils::select_folder("Select Folder Containing CSV Files to Format");
            if (!p.empty() && fs::exists(p)) {
                std::cout << "Selected folder: " << p.string() << "\n";
                bool create_all = prompt_yes_no_default_yes("Create all schema columns (keep unmapped empty)? (Y/n) [default Y]: ");
                do_bulk_dir(p, true, 0, 0, true, config, !create_all);
            } else {
                std::cout << "No folder selected (operation cancelled).\n\n";
            }
        } else if (choice_str == "4") {
            std::cout << "\nOpening File Explorer to select folder...\n";
            fs::path p = sl::DialogUtils::select_folder("Select Folder Containing CSV Files to Split");
            if (!p.empty() && fs::exists(p)) {
                std::cout << "Selected folder: " << p.string() << "\n\n";
                std::cout << "Choose split method:\n";
                std::cout << "  [1] By Maximum Rows per file (e.g. 1000 rows/file) [Default]\n";
                std::cout << "  [2] Into N equal parts (e.g. 2 parts)\n";
                std::cout << "Enter split method (1/2) [default 1]: ";
                std::string mode_str;
                std::getline(std::cin, mode_str);
                mode_str = sl::CSVReader::trim(mode_str);

                size_t parts = 0;
                size_t max_rows = 0;

                if (mode_str == "2") {
                    std::cout << "Enter number of parts [default 2]: ";
                    std::string parts_str;
                    std::getline(std::cin, parts_str);
                    parts = 2;
                    try {
                        if (!parts_str.empty()) parts = std::stoul(parts_str);
                    } catch (...) { parts = 2; }
                    if (parts == 0) parts = 2;
                } else {
                    std::cout << "Enter max data rows per file [default 1000]: ";
                    std::string rows_str;
                    std::getline(std::cin, rows_str);
                    max_rows = 1000;
                    try {
                        if (!rows_str.empty()) max_rows = std::stoul(rows_str);
                    } catch (...) { max_rows = 1000; }
                    if (max_rows == 0) max_rows = 1000;
                }

                bool keep_headers = prompt_yes_no_default_yes("Keep header in all split files? (Y/n) [default Y]: ");
                do_bulk_dir(p, false, parts, max_rows, keep_headers, config);
            } else {
                std::cout << "No folder selected (operation cancelled).\n\n";
            }
        } else if (choice_str == "5") {
            std::cout << "\n--- Target 35 Schema Columns ---\n";
            for (size_t i = 0; i < config.target_headers.size(); ++i) {
                std::cout << std::setw(2) << (i + 1) << ". " << config.target_headers[i] << "\n";
            }
            std::cout << "\n--- Configured Alias Mappings ---\n";
            for (const auto& [target, aliases] : config.mappings) {
                std::cout << target << " <- [";
                for (size_t a = 0; a < aliases.size(); ++a) {
                    std::cout << aliases[a] << (a + 1 < aliases.size() ? ", " : "");
                }
                std::cout << "]\n";
            }
            std::cout << "\n(Aliases can be edited in mapping_config.json)\n\n";
        }

        std::cout << "Press Enter to return to menu...";
        std::string dummy;
        std::getline(std::cin, dummy);
    }
}

int main(int argc, char* argv[]) {
    enable_utf8_console();

    fs::path config_file = "mapping_config.json";
    sl::SchemaConfig config = sl::ConfigManager::load_or_create_default(config_file);

    if (argc < 2) {
        interactive_menu(config);
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_help();
        return 0;
    }

    if (cmd == "interactive") {
        interactive_menu(config);
        return 0;
    }

    if (cmd == "init-config") {
        if (sl::ConfigManager::save_to_file(sl::SchemaConfig::get_default(), config_file)) {
            std::cout << "Created default config file: " << config_file.string() << "\n";
        } else {
            std::cerr << "Failed to write config file.\n";
            return 1;
        }
        return 0;
    }

    if (cmd == "format" || cmd == "transform") {
        if (argc < 3) {
            std::cerr << "Error: format requires an input CSV file.\n";
            std::cerr << "Usage: slman format <input.csv> [-o <output.csv>] [-c <config.json>] [--drop-empty]\n";
            return 1;
        }

        fs::path input_file = clean_input_path(argv[2]);
        fs::path output_file;
        bool drop_empty = false;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                output_file = clean_input_path(argv[++i]);
            } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                fs::path custom_cfg = clean_input_path(argv[++i]);
                config = sl::ConfigManager::load_or_create_default(custom_cfg);
            } else if (arg == "--drop-empty" || arg == "--prune-empty") {
                drop_empty = true;
            } else if (arg == "--all-columns" || arg == "--keep-all") {
                drop_empty = false;
            }
        }

        return do_format_file(input_file, output_file, config, drop_empty) ? 0 : 1;
    }

    if (cmd == "split") {
        if (argc < 3) {
            std::cerr << "Error: split requires an input CSV file.\n";
            std::cerr << "Usage: slman split <input.csv> [--parts <N>] [--max-rows <R>] [--no-headers] [--backup]\n";
            return 1;
        }

        fs::path input_file = clean_input_path(argv[2]);
        size_t parts = 2;
        size_t max_rows = 0;
        bool keep_headers = true;
        bool backup = false;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--parts" && i + 1 < argc) {
                parts = std::stoul(argv[++i]);
            } else if (arg == "--max-rows" && i + 1 < argc) {
                max_rows = std::stoul(argv[++i]);
            } else if (arg == "--no-headers") {
                keep_headers = false;
            } else if (arg == "--keep-headers" && i + 1 < argc) {
                std::string val = sl::CSVReader::to_lower(argv[++i]);
                keep_headers = (val == "y" || val == "yes" || val == "true" || val == "1");
            } else if (arg == "--backup" || arg == "--move-original") {
                backup = true;
            }
        }

        return do_split_file(input_file, parts, max_rows, keep_headers, backup) ? 0 : 1;
    }

    if (cmd == "bulk") {
        fs::path dir_path = fs::current_path();
        bool format_mode = false;
        size_t split_parts = 0;
        size_t split_max_rows = 0;
        bool keep_headers = true;
        bool drop_empty = false;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--format" || arg == "--transform") {
                format_mode = true;
            } else if (arg == "--drop-empty" || arg == "--prune-empty") {
                drop_empty = true;
            } else if (arg == "--all-columns" || arg == "--keep-all") {
                drop_empty = false;
            } else if ((arg == "--max-rows" || arg == "--split-rows" || arg == "--rows") && i + 1 < argc) {
                split_max_rows = std::stoul(argv[++i]);
            } else if (arg == "--parts" && i + 1 < argc) {
                split_parts = std::stoul(argv[++i]);
            } else if (arg == "--split") {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    split_parts = std::stoul(argv[++i]);
                } else if (split_max_rows == 0 && split_parts == 0) {
                    split_parts = 2;
                }
            } else if (arg == "--no-headers") {
                keep_headers = false;
            } else if (arg == "--keep-headers" && i + 1 < argc) {
                std::string val = sl::CSVReader::to_lower(argv[++i]);
                keep_headers = (val == "y" || val == "yes" || val == "true" || val == "1");
            } else if (arg[0] != '-') {
                dir_path = clean_input_path(arg);
            }
        }

        if (!format_mode && split_parts == 0 && split_max_rows == 0) {
            format_mode = true; // Default bulk action is format
        }

        do_bulk_dir(dir_path, format_mode, split_parts, split_max_rows, keep_headers, config, drop_empty);
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    print_help();
    return 1;
}
