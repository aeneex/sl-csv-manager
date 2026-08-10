#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <thread>
#include <atomic>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

#include "csv_engine.hpp"
#include "csv_splitter.hpp"
#include "schema_transformer.hpp"
#include "config_manager.hpp"
#include "dialog_utils.hpp"

namespace fs = std::filesystem;

// Custom Messages
#define WM_APP_LOG        (WM_APP + 101)
#define WM_APP_PROGRESS   (WM_APP + 102)
#define WM_APP_COMPLETED  (WM_APP + 103)

// Control IDs
enum ControlID {
    ID_TAB_FORMAT = 1001,
    ID_TAB_SPLIT = 1002,
    ID_TAB_MANAGE = 1003,
    ID_BTN_CLEAR_LOG = 1004,

    // Format Controls
    ID_FMT_MODE_FILE = 1101,
    ID_FMT_MODE_FOLDER = 1102,
    ID_FMT_EDIT_PATH = 1103,
    ID_FMT_BTN_BROWSE = 1104,
    ID_FMT_CHK_DROP_EMPTY = 1105,
    ID_FMT_BTN_RUN = 1106,

    // Split Controls
    ID_SPLIT_MODE_FILE = 1201,
    ID_SPLIT_MODE_FOLDER = 1202,
    ID_SPLIT_EDIT_PATH = 1203,
    ID_SPLIT_BTN_BROWSE = 1204,
    ID_SPLIT_TYPE_PARTS = 1205,
    ID_SPLIT_TYPE_ROWS = 1206,
    ID_SPLIT_EDIT_COUNT = 1207,
    ID_SPLIT_CHK_KEEP_HEADER = 1208,
    ID_SPLIT_BTN_RUN = 1209,

    // Manage Mappings Controls
    ID_MAP_LIST_COLUMNS = 1401,
    ID_MAP_EDIT_NEW_COL = 1402,
    ID_MAP_BTN_ADD_COL = 1403,
    ID_MAP_BTN_DEL_COL = 1404,
    ID_MAP_BTN_MOVE_UP = 1405,
    ID_MAP_BTN_MOVE_DOWN = 1406,
    ID_MAP_EDIT_MOVE_TO = 1407,
    ID_MAP_BTN_MOVE_TO = 1408,
    ID_MAP_EDIT_ALIASES = 1409,
    ID_MAP_BTN_SAVE = 1410,
    ID_MAP_BTN_RESET = 1411,
    ID_MAP_BTN_OPEN_FILE = 1412,

    // Common
    ID_EDIT_LOG = 1301,
    ID_STATIC_STATUS = 1302
};

// Pure Strict Monotone (Black, White, and Grays Only)
namespace Theme {
    const COLORREF BG_BLACK        = RGB(14, 14, 14);      // Pure dark background (#0E0E0E)
    const COLORREF PANEL_DARK      = RGB(22, 22, 22);      // Card container background (#161616)
    const COLORREF BORDER_GRAY     = RGB(42, 42, 42);      // Crisp 1px border (#2A2A2A)
    const COLORREF BORDER_LIGHT    = RGB(70, 70, 70);      // Highlighted border (#464646)
    const COLORREF INPUT_DARK      = RGB(28, 28, 28);      // Input fields background (#1C1C1C)
    const COLORREF INPUT_BORDER    = RGB(55, 55, 55);      // Input fields border (#373737)
    const COLORREF TEXT_WHITE      = RGB(255, 255, 255);   // High-contrast pure white
    const COLORREF TEXT_GRAY       = RGB(175, 175, 175);   // Secondary text
    const COLORREF TEXT_MUTED      = RGB(115, 115, 115);   // Muted hints
    const COLORREF BTN_PRIMARY_BG  = RGB(255, 255, 255);   // Pure white action button
    const COLORREF BTN_PRIMARY_TXT = RGB(0, 0, 0);          // Pitch black text
    const COLORREF BTN_SEC_BG      = RGB(30, 30, 30);      // Secondary button background
    const COLORREF BTN_SEC_BORDER  = RGB(55, 55, 55);      // Secondary button border
    const COLORREF BTN_SEC_TXT     = RGB(230, 230, 230);   // Secondary button text
    const COLORREF TAB_ACTIVE_BG   = RGB(42, 42, 42);      // Active tab
    const COLORREF TAB_INACTIVE_BG = RGB(18, 18, 18);      // Inactive tab
    const COLORREF LOG_BG          = RGB(8, 8, 8);         // Activity log background (#080808)
    const COLORREF LOG_TXT         = RGB(210, 210, 210);   // Activity log text
    const COLORREF PROG_BG         = RGB(24, 24, 24);      // Progress bar background
    const COLORREF PROG_FILL       = RGB(240, 240, 240);   // Monotone progress bar fill
}

struct AppState {
    HINSTANCE hInstance = nullptr;
    HWND hWndMain = nullptr;

    // Fonts
    HFONT hFontTitle = nullptr;
    HFONT hFontSub = nullptr;
    HFONT hFontBold = nullptr;
    HFONT hFontMedium = nullptr;
    HFONT hFontAction = nullptr;
    HFONT hFontLog = nullptr;

    // GDI Brushes & Pens
    HBRUSH hBrushBg = nullptr;
    HBRUSH hBrushPanel = nullptr;
    HBRUSH hBrushInput = nullptr;
    HBRUSH hBrushLog = nullptr;
    HPEN hPenBorder = nullptr;
    HPEN hPenInputBorder = nullptr;

    int activeTab = 0; // 0: Format, 1: Split, 2: Manage Mappings
    bool fmtIsFolder = false;
    bool fmtDropEmpty = true;

    bool splitIsFolder = false;
    bool splitIsPartsMode = true;
    bool splitKeepHeader = true;

    // Static Labels (for precise background painting)
    HWND hTitle = nullptr;
    HWND hSub = nullptr;
    HWND hLblFmt = nullptr;
    HWND hLblSplit = nullptr;
    HWND hLblMapCols = nullptr;
    HWND hLblMapAliases = nullptr;

    // Tab Buttons
    HWND hBtnTabFormat = nullptr;
    HWND hBtnTabSplit = nullptr;
    HWND hBtnTabManage = nullptr;

    // Format Controls
    std::vector<HWND> formatControls;
    HWND hFmtBtnFile = nullptr;
    HWND hFmtBtnFolder = nullptr;
    HWND hFmtEditPath = nullptr;
    HWND hFmtBtnBrowse = nullptr;
    HWND hFmtChkDropEmpty = nullptr;
    HWND hFmtBtnRun = nullptr;

    // Split Controls
    std::vector<HWND> splitControls;
    HWND hSplitBtnFile = nullptr;
    HWND hSplitBtnFolder = nullptr;
    HWND hSplitEditPath = nullptr;
    HWND hSplitBtnBrowse = nullptr;
    HWND hSplitBtnParts = nullptr;
    HWND hSplitBtnRows = nullptr;
    HWND hSplitEditCount = nullptr;
    HWND hSplitChkKeepHeader = nullptr;
    HWND hSplitBtnRun = nullptr;

    // Manage Mappings Controls
    std::vector<HWND> manageControls;
    HWND hMapListColumns = nullptr;
    HWND hMapEditNewCol = nullptr;
    HWND hMapBtnAddCol = nullptr;
    HWND hMapBtnDelCol = nullptr;
    HWND hMapBtnMoveUp = nullptr;
    HWND hMapBtnMoveDown = nullptr;
    HWND hMapEditMoveTo = nullptr;
    HWND hMapBtnMoveTo = nullptr;
    HWND hMapEditAliases = nullptr;
    HWND hMapBtnSave = nullptr;
    HWND hMapBtnReset = nullptr;
    HWND hMapBtnOpenFile = nullptr;
    int lastSelectedColIdx = -1;

    // Bottom Controls
    HWND hStaticStatus = nullptr;
    HWND hBtnClearLog = nullptr;
    HWND hEditLog = nullptr;

    // Progress State
    int progressCurrent = 0;
    int progressTotal = 0;

    std::atomic<bool> isRunning{false};
    sl::SchemaConfig config;
    sl::SchemaConfig editingConfig;
};

static AppState g_app;

static std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

static std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

static void post_log(const std::string& msg) {
    std::string* pMsg = new std::string(msg);
    PostMessage(g_app.hWndMain, WM_APP_LOG, 0, (LPARAM)pMsg);
}

static void post_progress(int current, int total) {
    PostMessage(g_app.hWndMain, WM_APP_PROGRESS, (WPARAM)current, (LPARAM)total);
}

static void post_completed(bool success) {
    PostMessage(g_app.hWndMain, WM_APP_COMPLETED, (WPARAM)(success ? 1 : 0), 0);
}

static void append_log(const std::string& text) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&in_time_t), "%H:%M:%S") << "] " << text << "\r\n";
    std::wstring wText = utf8_to_wstring(ss.str());

    int length = GetWindowTextLengthW(g_app.hEditLog);
    SendMessageW(g_app.hEditLog, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    SendMessageW(g_app.hEditLog, EM_REPLACESEL, FALSE, (LPARAM)wText.c_str());
    SendMessageW(g_app.hEditLog, EM_SCROLLCARET, 0, 0);
}

// Save current text in aliases box into editingConfig for the currently selected column
static void save_current_alias_box() {
    if (g_app.lastSelectedColIdx < 0 || g_app.lastSelectedColIdx >= (int)g_app.editingConfig.target_headers.size()) {
        return;
    }
    std::string targetCol = g_app.editingConfig.target_headers[g_app.lastSelectedColIdx];

    int len = GetWindowTextLengthW(g_app.hMapEditAliases);
    std::vector<wchar_t> buf(len + 1, 0);
    GetWindowTextW(g_app.hMapEditAliases, buf.data(), len + 1);
    std::string text = wstring_to_utf8(buf.data());

    std::vector<std::string> aliases;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        std::stringstream line_ss(line);
        std::string token;
        while (std::getline(line_ss, token, ',')) {
            token = sl::CSVReader::trim(token);
            if (!token.empty()) {
                aliases.push_back(token);
            }
        }
    }

    g_app.editingConfig.mappings[targetCol] = aliases;
}

// Populate the Manage Mappings ListBox
static void refresh_manage_columns_list(int selectIdx = 0) {
    SendMessageW(g_app.hMapListColumns, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < g_app.editingConfig.target_headers.size(); ++i) {
        const std::string& col = g_app.editingConfig.target_headers[i];
        std::wstringstream wss;
        wss << (i + 1) << L". " << utf8_to_wstring(col);
        SendMessageW(g_app.hMapListColumns, LB_ADDSTRING, 0, (LPARAM)wss.str().c_str());
    }

    if (!g_app.editingConfig.target_headers.empty()) {
        if (selectIdx < 0) selectIdx = 0;
        if (selectIdx >= (int)g_app.editingConfig.target_headers.size()) {
            selectIdx = (int)g_app.editingConfig.target_headers.size() - 1;
        }
        SendMessageW(g_app.hMapListColumns, LB_SETCURSEL, (WPARAM)selectIdx, 0);
        g_app.lastSelectedColIdx = selectIdx;

        std::string col = g_app.editingConfig.target_headers[selectIdx];
        std::vector<std::string> aliases = g_app.editingConfig.mappings[col];
        std::wstringstream alias_ss;
        for (size_t i = 0; i < aliases.size(); ++i) {
            alias_ss << utf8_to_wstring(aliases[i]);
            if (i + 1 < aliases.size()) alias_ss << L", ";
        }
        SetWindowTextW(g_app.hMapEditAliases, alias_ss.str().c_str());
        SetWindowTextW(g_app.hMapEditMoveTo, std::to_wstring(selectIdx + 1).c_str());
    } else {
        SetWindowTextW(g_app.hMapEditAliases, L"");
        SetWindowTextW(g_app.hMapEditMoveTo, L"");
        g_app.lastSelectedColIdx = -1;
    }
}

static void update_tab_visibility() {
    for (HWND h : g_app.formatControls) {
        ShowWindow(h, g_app.activeTab == 0 ? SW_SHOW : SW_HIDE);
    }
    for (HWND h : g_app.splitControls) {
        ShowWindow(h, g_app.activeTab == 1 ? SW_SHOW : SW_HIDE);
    }
    for (HWND h : g_app.manageControls) {
        ShowWindow(h, g_app.activeTab == 2 ? SW_SHOW : SW_HIDE);
    }

    if (g_app.activeTab == 2) {
        g_app.editingConfig = sl::ConfigManager::load_or_create_default("mapping_config.json");
        refresh_manage_columns_list(0);
    }

    InvalidateRect(g_app.hWndMain, NULL, TRUE);
}

static void set_ui_busy(bool busy) {
    EnableWindow(g_app.hFmtBtnRun, !busy);
    EnableWindow(g_app.hSplitBtnRun, !busy);
    EnableWindow(g_app.hFmtBtnBrowse, !busy);
    EnableWindow(g_app.hSplitBtnBrowse, !busy);
    EnableWindow(g_app.hBtnTabFormat, !busy);
    EnableWindow(g_app.hBtnTabSplit, !busy);
    EnableWindow(g_app.hBtnTabManage, !busy);

    if (busy) {
        SetWindowTextW(g_app.hStaticStatus, L"Status: Processing in background...");
        g_app.progressCurrent = 0;
        g_app.progressTotal = 100;
    } else {
        SetWindowTextW(g_app.hStaticStatus, L"Status: Ready");
    }
    InvalidateRect(g_app.hWndMain, NULL, TRUE);
}

// Background Worker: Format Task
static void run_format_task(std::string input_path_str, bool is_folder, bool drop_empty) {
    sl::SchemaConfig config = sl::ConfigManager::load_or_create_default("mapping_config.json");
    fs::path input_path(utf8_to_wstring(input_path_str));

    if (!fs::exists(input_path)) {
        post_log("Error: Path does not exist: " + input_path_str);
        post_completed(false);
        return;
    }

    if (!is_folder) {
        post_log("--> Formatting single CSV: " + input_path.filename().string());
        fs::path parent = input_path.has_parent_path() ? input_path.parent_path() : fs::current_path();
        fs::path format_done_dir = parent / "format done";
        std::error_code ec;
        if (!fs::exists(format_done_dir)) {
            fs::create_directories(format_done_dir, ec);
        }
        fs::path output_path = format_done_dir / input_path.filename();

        sl::SchemaTransformer transformer(config);
        sl::TransformStats stats;
        if (transformer.transform_file(input_path, output_path, &stats, drop_empty)) {
            post_progress(100, 100);
            post_log(" [Success] Formatted " + std::to_string(stats.rows_processed) + " rows.");
            post_log("           Mapped columns: " + std::to_string(stats.columns_mapped) + " / " + std::to_string(stats.total_target_columns));
            post_log("           Saved into: " + output_path.string());
            post_completed(true);
        } else {
            post_log(" [Error] Failed to format file: " + input_path.string());
            post_completed(false);
        }
    } else {
        post_log("--> Bulk formatting folder: " + input_path.string());
        std::vector<fs::path> csv_files;
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (entry.is_regular_file() && sl::CSVReader::to_lower(entry.path().extension().string()) == ".csv") {
                std::string parent_name = entry.path().parent_path().filename().string();
                if (parent_name == "original files" || parent_name == "format done" || parent_name == "split done" || parent_name == "done") continue;
                csv_files.push_back(entry.path());
            }
        }

        if (csv_files.empty()) {
            post_log(" [Info] No CSV files found in " + input_path.string());
            post_completed(true);
            return;
        }

        post_log("Found " + std::to_string(csv_files.size()) + " CSV file(s) to format.");
        size_t processed = 0;
        size_t success_count = 0;

        for (const auto& csv : csv_files) {
            fs::path parent = csv.has_parent_path() ? csv.parent_path() : fs::current_path();
            fs::path format_done_dir = parent / "format done";
            std::error_code ec;
            if (!fs::exists(format_done_dir)) {
                fs::create_directories(format_done_dir, ec);
            }
            fs::path output_path = format_done_dir / csv.filename();

            sl::SchemaTransformer transformer(config);
            sl::TransformStats stats;
            if (transformer.transform_file(csv, output_path, &stats, drop_empty)) {
                success_count++;
                post_log(" [OK] Formatted: " + csv.filename().string() + " (" + std::to_string(stats.rows_processed) + " rows)");
            } else {
                post_log(" [Failed] Error formatting: " + csv.filename().string());
            }
            processed++;
            post_progress((int)processed, (int)csv_files.size());
        }

        post_log("=================================================");
        post_log("Bulk Format Finished: " + std::to_string(success_count) + " / " + std::to_string(csv_files.size()) + " files formatted.");
        post_completed(success_count == csv_files.size());
    }
}

// Background Worker: Split Task
static void run_split_task(std::string input_path_str, bool is_folder, bool is_parts_mode, size_t count_val, bool keep_header) {
    fs::path input_path(utf8_to_wstring(input_path_str));

    if (!fs::exists(input_path)) {
        post_log("Error: Path does not exist: " + input_path_str);
        post_completed(false);
        return;
    }

    if (!is_folder) {
        post_log("--> Splitting single CSV: " + input_path.filename().string());
        sl::SplitResult res;
        if (is_parts_mode) {
            res = sl::CSVSplitter::split_into_parts(input_path, count_val, keep_header, false);
        } else {
            res = sl::CSVSplitter::split_by_max_rows(input_path, count_val, keep_header, false);
        }

        if (res.success) {
            post_progress(100, 100);
            post_log(" [Success] Split " + std::to_string(res.total_data_rows) + " rows into " + std::to_string(res.parts_created) + " parts.");
            post_log("           Saved into: 'split done/' subfolder");
            for (const auto& out_file : res.output_files) {
                post_log("           * " + out_file.filename().string());
            }
            post_completed(true);
        } else {
            post_log(" [Error] " + res.error_message);
            post_completed(false);
        }
    } else {
        post_log("--> Bulk splitting folder: " + input_path.string());
        std::vector<fs::path> csv_files;
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (entry.is_regular_file() && sl::CSVReader::to_lower(entry.path().extension().string()) == ".csv") {
                std::string parent_name = entry.path().parent_path().filename().string();
                if (parent_name == "original files" || parent_name == "format done" || parent_name == "split done" || parent_name == "done") continue;
                csv_files.push_back(entry.path());
            }
        }

        if (csv_files.empty()) {
            post_log(" [Info] No CSV files found in " + input_path.string());
            post_completed(true);
            return;
        }

        post_log("Found " + std::to_string(csv_files.size()) + " CSV file(s) to split.");
        size_t processed = 0;
        size_t success_count = 0;

        for (const auto& csv : csv_files) {
            sl::SplitResult res;
            if (is_parts_mode) {
                res = sl::CSVSplitter::split_into_parts(csv, count_val, keep_header, false);
            } else {
                res = sl::CSVSplitter::split_by_max_rows(csv, count_val, keep_header, false);
            }

            if (res.success) {
                success_count++;
                post_log(" [OK] Split: " + csv.filename().string() + " into " + std::to_string(res.parts_created) + " files");
            } else {
                post_log(" [Failed] Error splitting: " + csv.filename().string());
            }
            processed++;
            post_progress((int)processed, (int)csv_files.size());
        }

        post_log("=================================================");
        post_log("Bulk Split Finished: " + std::to_string(success_count) + " / " + std::to_string(csv_files.size()) + " files split.");
        post_completed(success_count == csv_files.size());
    }
}

// Custom Draw Helper: 100% Flat Monotone Sharp Button
static void draw_flat_button(DRAWITEMSTRUCT* pDIS, const wchar_t* text, bool isPrimary, bool isActiveToggle = false) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;
    bool isPressed = (pDIS->itemState & ODS_SELECTED);
    bool isDisabled = (pDIS->itemState & ODS_DISABLED);

    COLORREF bgColor;
    COLORREF txtColor;
    COLORREF borderColor;

    if (isPrimary) {
        bgColor = isPressed ? RGB(200, 200, 200) : (isDisabled ? RGB(60, 60, 60) : Theme::BTN_PRIMARY_BG);
        txtColor = isDisabled ? RGB(140, 140, 140) : Theme::BTN_PRIMARY_TXT;
        borderColor = bgColor;
    } else if (isActiveToggle) {
        bgColor = Theme::TAB_ACTIVE_BG;
        txtColor = Theme::TEXT_WHITE;
        borderColor = Theme::BORDER_LIGHT;
    } else {
        bgColor = isPressed ? RGB(45, 45, 45) : Theme::BTN_SEC_BG;
        txtColor = isDisabled ? Theme::TEXT_MUTED : Theme::BTN_SEC_TXT;
        borderColor = Theme::BTN_SEC_BORDER;
    }

    // Fill flat solid rectangle
    HBRUSH hBr = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, hBr);
    DeleteObject(hBr);

    // Draw crisp 1px border
    HBRUSH hBorderBr = CreateSolidBrush(borderColor);
    FrameRect(hdc, &rc, hBorderBr);
    DeleteObject(hBorderBr);

    // Draw Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, txtColor);
    SelectObject(hdc, isPrimary ? g_app.hFontAction : g_app.hFontBold);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Custom Draw Helper: 100% Flat Monotone Checkbox Toggle
static void draw_flat_checkbox(DRAWITEMSTRUCT* pDIS, const wchar_t* labelText, bool isChecked) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    // Fill Panel Background
    FillRect(hdc, &rc, g_app.hBrushPanel);

    // Draw Check Box Square (16x16)
    RECT rcBox = { rc.left, rc.top + (rc.bottom - rc.top - 16) / 2, rc.left + 16, rc.top + (rc.bottom - rc.top - 16) / 2 + 16 };
    HBRUSH hBoxBr = CreateSolidBrush(isChecked ? Theme::TEXT_WHITE : Theme::INPUT_DARK);
    FillRect(hdc, &rcBox, hBoxBr);
    DeleteObject(hBoxBr);

    HBRUSH hBoxBorder = CreateSolidBrush(isChecked ? Theme::TEXT_WHITE : Theme::BORDER_LIGHT);
    FrameRect(hdc, &rcBox, hBoxBorder);
    DeleteObject(hBoxBorder);

    // If checked, draw dark square mark inside
    if (isChecked) {
        RECT rcInner = { rcBox.left + 4, rcBox.top + 4, rcBox.right - 4, rcBox.bottom - 4 };
        HBRUSH hMark = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rcInner, hMark);
        DeleteObject(hMark);
    }

    // Draw Label Text
    RECT rcText = { rcBox.right + 10, rc.top, rc.right, rc.bottom };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isChecked ? Theme::TEXT_WHITE : Theme::TEXT_GRAY);
    SelectObject(hdc, g_app.hFontMedium);
    DrawTextW(hdc, labelText, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Enable Immersive Dark Mode for Window Title Bar (Windows 10/11)
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
        DwmSetWindowAttribute(hWnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_OLD */, &darkMode, sizeof(darkMode));

        // Typography
        g_app.hFontTitle = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.hFontSub = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.hFontBold = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.hFontMedium = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.hFontAction = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.hFontLog = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        // GDI Brushes & Pens
        g_app.hBrushBg = CreateSolidBrush(Theme::BG_BLACK);
        g_app.hBrushPanel = CreateSolidBrush(Theme::PANEL_DARK);
        g_app.hBrushInput = CreateSolidBrush(Theme::INPUT_DARK);
        g_app.hBrushLog = CreateSolidBrush(Theme::LOG_BG);
        g_app.hPenBorder = CreatePen(PS_SOLID, 1, Theme::BORDER_GRAY);
        g_app.hPenInputBorder = CreatePen(PS_SOLID, 1, Theme::INPUT_BORDER);

        // 1. Header Banner
        g_app.hTitle = CreateWindowW(L"STATIC", L"SearchLeads CSV Manager",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 30, 20, 450, 28, hWnd, NULL, g_app.hInstance, NULL);
        SendMessage(g_app.hTitle, WM_SETFONT, (WPARAM)g_app.hFontTitle, TRUE);

        g_app.hSub = CreateWindowW(L"STATIC", L"CSV Formatter & Splitter  |  Drag and drop files anytime",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 30, 52, 500, 20, hWnd, NULL, g_app.hInstance, NULL);
        SendMessage(g_app.hSub, WM_SETFONT, (WPARAM)g_app.hFontSub, TRUE);

        // 2. Tab Buttons (3 Flat Segmented Tabs: Format, Split, Manage Mappings)
        g_app.hBtnTabFormat = CreateWindowW(L"BUTTON", L"Format CSV",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 30, 85, 235, 42, hWnd, (HMENU)ID_TAB_FORMAT, g_app.hInstance, NULL);

        g_app.hBtnTabSplit = CreateWindowW(L"BUTTON", L"Split CSV",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 275, 85, 235, 42, hWnd, (HMENU)ID_TAB_SPLIT, g_app.hInstance, NULL);

        g_app.hBtnTabManage = CreateWindowW(L"BUTTON", L"Manage Mappings",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 520, 85, 230, 42, hWnd, (HMENU)ID_TAB_MANAGE, g_app.hInstance, NULL);

        // -------------------------------------------------------------
        // TAB 0: FORMAT CONTROLS (Inside Card Panel Y: 135 to 480)
        // -------------------------------------------------------------
        g_app.hFmtBtnFile = CreateWindowW(L"BUTTON", L"Single File",
            WS_CHILD | BS_OWNERDRAW, 50, 160, 160, 36, hWnd, (HMENU)ID_FMT_MODE_FILE, g_app.hInstance, NULL);
        g_app.formatControls.push_back(g_app.hFmtBtnFile);

        g_app.hFmtBtnFolder = CreateWindowW(L"BUTTON", L"Entire Folder (Bulk)",
            WS_CHILD | BS_OWNERDRAW, 220, 160, 180, 36, hWnd, (HMENU)ID_FMT_MODE_FOLDER, g_app.hInstance, NULL);
        g_app.formatControls.push_back(g_app.hFmtBtnFolder);

        g_app.hLblFmt = CreateWindowW(L"STATIC", L"Target Path:",
            WS_CHILD | SS_LEFT, 50, 220, 300, 20, hWnd, NULL, g_app.hInstance, NULL);
        SendMessage(g_app.hLblFmt, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);
        g_app.formatControls.push_back(g_app.hLblFmt);

        g_app.hFmtEditPath = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 50, 248, 530, 36, hWnd, (HMENU)ID_FMT_EDIT_PATH, g_app.hInstance, NULL);
        SendMessage(g_app.hFmtEditPath, WM_SETFONT, (WPARAM)g_app.hFontMedium, TRUE);
        g_app.formatControls.push_back(g_app.hFmtEditPath);

        g_app.hFmtBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...",
            WS_CHILD | BS_OWNERDRAW, 590, 248, 130, 36, hWnd, (HMENU)ID_FMT_BTN_BROWSE, g_app.hInstance, NULL);
        g_app.formatControls.push_back(g_app.hFmtBtnBrowse);

        g_app.hFmtChkDropEmpty = CreateWindowW(L"BUTTON", L"Automatically delete completely empty columns",
            WS_CHILD | BS_OWNERDRAW, 50, 312, 450, 26, hWnd, (HMENU)ID_FMT_CHK_DROP_EMPTY, g_app.hInstance, NULL);
        g_app.formatControls.push_back(g_app.hFmtChkDropEmpty);

        g_app.hFmtBtnRun = CreateWindowW(L"BUTTON", L"Format to Technical Schema",
            WS_CHILD | BS_OWNERDRAW, 50, 395, 670, 48, hWnd, (HMENU)ID_FMT_BTN_RUN, g_app.hInstance, NULL);
        g_app.formatControls.push_back(g_app.hFmtBtnRun);

        // -------------------------------------------------------------
        // TAB 1: SPLIT CONTROLS (Inside Card Panel Y: 135 to 480)
        // -------------------------------------------------------------
        g_app.hSplitBtnFile = CreateWindowW(L"BUTTON", L"Single File",
            WS_CHILD | BS_OWNERDRAW, 50, 160, 160, 36, hWnd, (HMENU)ID_SPLIT_MODE_FILE, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitBtnFile);

        g_app.hSplitBtnFolder = CreateWindowW(L"BUTTON", L"Entire Folder (Bulk)",
            WS_CHILD | BS_OWNERDRAW, 220, 160, 180, 36, hWnd, (HMENU)ID_SPLIT_MODE_FOLDER, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitBtnFolder);

        g_app.hLblSplit = CreateWindowW(L"STATIC", L"Target Path:",
            WS_CHILD | SS_LEFT, 50, 220, 300, 20, hWnd, NULL, g_app.hInstance, NULL);
        SendMessage(g_app.hLblSplit, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);
        g_app.splitControls.push_back(g_app.hLblSplit);

        g_app.hSplitEditPath = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 50, 248, 530, 36, hWnd, (HMENU)ID_SPLIT_EDIT_PATH, g_app.hInstance, NULL);
        SendMessage(g_app.hSplitEditPath, WM_SETFONT, (WPARAM)g_app.hFontMedium, TRUE);
        g_app.splitControls.push_back(g_app.hSplitEditPath);

        g_app.hSplitBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...",
            WS_CHILD | BS_OWNERDRAW, 590, 248, 130, 36, hWnd, (HMENU)ID_SPLIT_BTN_BROWSE, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitBtnBrowse);

        // Split Type Selector
        g_app.hSplitBtnParts = CreateWindowW(L"BUTTON", L"Split into N parts",
            WS_CHILD | BS_OWNERDRAW, 50, 312, 150, 32, hWnd, (HMENU)ID_SPLIT_TYPE_PARTS, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitBtnParts);

        g_app.hSplitBtnRows = CreateWindowW(L"BUTTON", L"Max rows per part",
            WS_CHILD | BS_OWNERDRAW, 208, 312, 150, 32, hWnd, (HMENU)ID_SPLIT_TYPE_ROWS, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitBtnRows);

        g_app.hSplitEditCount = CreateWindowExW(0, L"EDIT", L"2",
            WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_CENTER, 368, 312, 55, 32, hWnd, (HMENU)ID_SPLIT_EDIT_COUNT, g_app.hInstance, NULL);
        SendMessage(g_app.hSplitEditCount, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);
        g_app.splitControls.push_back(g_app.hSplitEditCount);

        g_app.hSplitChkKeepHeader = CreateWindowW(L"BUTTON", L"Keep header in all split files",
            WS_CHILD | BS_OWNERDRAW, 440, 315, 260, 26, hWnd, (HMENU)ID_SPLIT_CHK_KEEP_HEADER, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitChkKeepHeader);

        g_app.hSplitBtnRun = CreateWindowW(L"BUTTON", L"Split CSV Files",
            WS_CHILD | BS_OWNERDRAW, 50, 395, 670, 48, hWnd, (HMENU)ID_SPLIT_BTN_RUN, g_app.hInstance, NULL);
        g_app.splitControls.push_back(g_app.hSplitBtnRun);

        // -------------------------------------------------------------
        // TAB 2: MANAGE MAPPINGS CONTROLS (Inside Card Panel Y: 135 to 480)
        // -------------------------------------------------------------
        // Left Column: Columns List & Management
        g_app.hLblMapCols = CreateWindowW(L"STATIC", L"Target Output Columns:",
            WS_CHILD | SS_LEFT, 50, 150, 280, 20, hWnd, NULL, g_app.hInstance, NULL);
        SendMessage(g_app.hLblMapCols, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);
        g_app.manageControls.push_back(g_app.hLblMapCols);

        g_app.hMapListColumns = CreateWindowExW(0, L"LISTBOX", L"",
            WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            50, 175, 300, 180, hWnd, (HMENU)ID_MAP_LIST_COLUMNS, g_app.hInstance, NULL);
        SendMessage(g_app.hMapListColumns, WM_SETFONT, (WPARAM)g_app.hFontMedium, TRUE);
        g_app.manageControls.push_back(g_app.hMapListColumns);

        g_app.hMapEditNewCol = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 50, 365, 195, 28, hWnd, (HMENU)ID_MAP_EDIT_NEW_COL, g_app.hInstance, NULL);
        SendMessage(g_app.hMapEditNewCol, WM_SETFONT, (WPARAM)g_app.hFontMedium, TRUE);
        g_app.manageControls.push_back(g_app.hMapEditNewCol);

        g_app.hMapBtnAddCol = CreateWindowW(L"BUTTON", L"+ Add Column",
            WS_CHILD | BS_OWNERDRAW, 252, 365, 98, 28, hWnd, (HMENU)ID_MAP_BTN_ADD_COL, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnAddCol);

        g_app.hMapBtnDelCol = CreateWindowW(L"BUTTON", L"Delete",
            WS_CHILD | BS_OWNERDRAW, 50, 400, 90, 26, hWnd, (HMENU)ID_MAP_BTN_DEL_COL, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnDelCol);

        g_app.hMapBtnMoveUp = CreateWindowW(L"BUTTON", L"Move Up",
            WS_CHILD | BS_OWNERDRAW, 150, 400, 95, 26, hWnd, (HMENU)ID_MAP_BTN_MOVE_UP, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnMoveUp);

        g_app.hMapBtnMoveDown = CreateWindowW(L"BUTTON", L"Move Down",
            WS_CHILD | BS_OWNERDRAW, 255, 400, 95, 26, hWnd, (HMENU)ID_MAP_BTN_MOVE_DOWN, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnMoveDown);

        // Move to Position (Inline Index Edit & Button)
        g_app.hMapEditMoveTo = CreateWindowExW(0, L"EDIT", L"1",
            WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_CENTER, 50, 434, 55, 28, hWnd, (HMENU)ID_MAP_EDIT_MOVE_TO, g_app.hInstance, NULL);
        SendMessage(g_app.hMapEditMoveTo, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);
        g_app.manageControls.push_back(g_app.hMapEditMoveTo);

        g_app.hMapBtnMoveTo = CreateWindowW(L"BUTTON", L"Move to Position #",
            WS_CHILD | BS_OWNERDRAW, 112, 434, 238, 28, hWnd, (HMENU)ID_MAP_BTN_MOVE_TO, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnMoveTo);

        // Right Column: Aliases Editing & Save
        g_app.hLblMapAliases = CreateWindowW(L"STATIC", L"Aliases (comma-separated or one per line):",
            WS_CHILD | SS_LEFT, 375, 150, 345, 20, hWnd, NULL, g_app.hInstance, NULL);
        SendMessage(g_app.hLblMapAliases, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);
        g_app.manageControls.push_back(g_app.hLblMapAliases);

        g_app.hMapEditAliases = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
            375, 175, 345, 180, hWnd, (HMENU)ID_MAP_EDIT_ALIASES, g_app.hInstance, NULL);
        SendMessage(g_app.hMapEditAliases, WM_SETFONT, (WPARAM)g_app.hFontMedium, TRUE);
        g_app.manageControls.push_back(g_app.hMapEditAliases);

        g_app.hMapBtnSave = CreateWindowW(L"BUTTON", L"Save to mapping_config.json",
            WS_CHILD | BS_OWNERDRAW, 375, 365, 345, 36, hWnd, (HMENU)ID_MAP_BTN_SAVE, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnSave);

        g_app.hMapBtnReset = CreateWindowW(L"BUTTON", L"Reset to Defaults",
            WS_CHILD | BS_OWNERDRAW, 375, 412, 165, 26, hWnd, (HMENU)ID_MAP_BTN_RESET, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnReset);

        g_app.hMapBtnOpenFile = CreateWindowW(L"BUTTON", L"Open JSON File",
            WS_CHILD | BS_OWNERDRAW, 550, 412, 170, 26, hWnd, (HMENU)ID_MAP_BTN_OPEN_FILE, g_app.hInstance, NULL);
        g_app.manageControls.push_back(g_app.hMapBtnOpenFile);

        // Show Format tab initially
        update_tab_visibility();

        // -------------------------------------------------------------
        // BOTTOM: STATUS, PROGRESS, LOG (Y: 490 to 760)
        // -------------------------------------------------------------
        g_app.hStaticStatus = CreateWindowW(L"STATIC", L"Status: Ready",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 30, 492, 480, 20, hWnd, (HMENU)ID_STATIC_STATUS, g_app.hInstance, NULL);
        SendMessage(g_app.hStaticStatus, WM_SETFONT, (WPARAM)g_app.hFontBold, TRUE);

        g_app.hBtnClearLog = CreateWindowW(L"BUTTON", L"Clear Log",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 650, 488, 100, 26, hWnd, (HMENU)ID_BTN_CLEAR_LOG, g_app.hInstance, NULL);

        // Log Console
        g_app.hEditLog = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            30, 532, 720, 200, hWnd, (HMENU)ID_EDIT_LOG, g_app.hInstance, NULL);
        SendMessage(g_app.hEditLog, WM_SETFONT, (WPARAM)g_app.hFontLog, TRUE);

        DragAcceptFiles(hWnd, TRUE);

        append_log("SLMAN initialized. Ready.");
        break;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        FillRect(hdc, &rcClient, g_app.hBrushBg);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        FillRect(hdc, &rcClient, g_app.hBrushBg);

        // Draw Card Panel (X: 30 to 750, Y: 135 to 476)
        RECT rcPanel = { 30, 135, 750, 476 };
        FillRect(hdc, &rcPanel, g_app.hBrushPanel);

        HBRUSH hBrBorder = CreateSolidBrush(Theme::BORDER_GRAY);
        FrameRect(hdc, &rcPanel, hBrBorder);
        DeleteObject(hBrBorder);

        // Draw Progress Bar (X: 30 to 750, Y: 520 to 526)
        RECT rcProgBg = { 30, 520, 750, 526 };
        HBRUSH hProgBg = CreateSolidBrush(Theme::PROG_BG);
        FillRect(hdc, &rcProgBg, hProgBg);
        DeleteObject(hProgBg);

        if (g_app.progressTotal > 0 && g_app.progressCurrent > 0) {
            int fillW = (int)(((double)g_app.progressCurrent / (double)g_app.progressTotal) * (rcProgBg.right - rcProgBg.left));
            if (fillW > (rcProgBg.right - rcProgBg.left)) fillW = rcProgBg.right - rcProgBg.left;
            RECT rcProgFill = { rcProgBg.left, rcProgBg.top, rcProgBg.left + fillW, rcProgBg.bottom };
            HBRUSH hProgFill = CreateSolidBrush(Theme::PROG_FILL);
            FillRect(hdc, &rcProgFill, hProgFill);
            DeleteObject(hProgFill);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
        int id = (int)pDIS->CtlID;

        if (id == ID_TAB_FORMAT) {
            draw_flat_button(pDIS, L"Format CSV", false, g_app.activeTab == 0);
            return TRUE;
        }
        if (id == ID_TAB_SPLIT) {
            draw_flat_button(pDIS, L"Split CSV", false, g_app.activeTab == 1);
            return TRUE;
        }
        if (id == ID_TAB_MANAGE) {
            draw_flat_button(pDIS, L"Manage Mappings", false, g_app.activeTab == 2);
            return TRUE;
        }
        if (id == ID_FMT_MODE_FILE) {
            draw_flat_button(pDIS, L"Single File", false, !g_app.fmtIsFolder);
            return TRUE;
        }
        if (id == ID_FMT_MODE_FOLDER) {
            draw_flat_button(pDIS, L"Entire Folder (Bulk)", false, g_app.fmtIsFolder);
            return TRUE;
        }
        if (id == ID_SPLIT_MODE_FILE) {
            draw_flat_button(pDIS, L"Single File", false, !g_app.splitIsFolder);
            return TRUE;
        }
        if (id == ID_SPLIT_MODE_FOLDER) {
            draw_flat_button(pDIS, L"Entire Folder (Bulk)", false, g_app.splitIsFolder);
            return TRUE;
        }
        if (id == ID_SPLIT_TYPE_PARTS) {
            draw_flat_button(pDIS, L"Split into N parts", false, g_app.splitIsPartsMode);
            return TRUE;
        }
        if (id == ID_SPLIT_TYPE_ROWS) {
            draw_flat_button(pDIS, L"Max rows per part", false, !g_app.splitIsPartsMode);
            return TRUE;
        }
        if (id == ID_FMT_BTN_BROWSE || id == ID_SPLIT_BTN_BROWSE) {
            draw_flat_button(pDIS, L"Browse...", false, false);
            return TRUE;
        }
        if (id == ID_BTN_CLEAR_LOG) {
            draw_flat_button(pDIS, L"Clear Log", false, false);
            return TRUE;
        }
        if (id == ID_FMT_CHK_DROP_EMPTY) {
            draw_flat_checkbox(pDIS, L"Automatically delete completely empty columns", g_app.fmtDropEmpty);
            return TRUE;
        }
        if (id == ID_SPLIT_CHK_KEEP_HEADER) {
            draw_flat_checkbox(pDIS, L"Keep header in all split files", g_app.splitKeepHeader);
            return TRUE;
        }
        if (id == ID_FMT_BTN_RUN) {
            draw_flat_button(pDIS, L"Format to Technical Schema", true, false);
            return TRUE;
        }
        if (id == ID_SPLIT_BTN_RUN) {
            draw_flat_button(pDIS, L"Split CSV Files", true, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_ADD_COL) {
            draw_flat_button(pDIS, L"+ Add Column", false, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_DEL_COL) {
            draw_flat_button(pDIS, L"Delete", false, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_MOVE_UP) {
            draw_flat_button(pDIS, L"Move Up", false, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_MOVE_DOWN) {
            draw_flat_button(pDIS, L"Move Down", false, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_MOVE_TO) {
            draw_flat_button(pDIS, L"Move to Position #", false, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_SAVE) {
            draw_flat_button(pDIS, L"Save to mapping_config.json", true, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_RESET) {
            draw_flat_button(pDIS, L"Reset to Defaults", false, false);
            return TRUE;
        }
        if (id == ID_MAP_BTN_OPEN_FILE) {
            draw_flat_button(pDIS, L"Open JSON File", false, false);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        // Tab Switching
        if (id == ID_TAB_FORMAT && code == BN_CLICKED) {
            if (g_app.activeTab == 2) save_current_alias_box();
            g_app.activeTab = 0;
            update_tab_visibility();
            return 0;
        }
        if (id == ID_TAB_SPLIT && code == BN_CLICKED) {
            if (g_app.activeTab == 2) save_current_alias_box();
            g_app.activeTab = 1;
            update_tab_visibility();
            return 0;
        }
        if (id == ID_TAB_MANAGE && code == BN_CLICKED) {
            g_app.activeTab = 2;
            update_tab_visibility();
            return 0;
        }

        // Manage Mappings: Column Selection Change
        if (id == ID_MAP_LIST_COLUMNS && code == LBN_SELCHANGE) {
            save_current_alias_box();
            int curSel = (int)SendMessageW(g_app.hMapListColumns, LB_GETCURSEL, 0, 0);
            if (curSel >= 0 && curSel < (int)g_app.editingConfig.target_headers.size()) {
                g_app.lastSelectedColIdx = curSel;
                std::string col = g_app.editingConfig.target_headers[curSel];
                std::vector<std::string> aliases = g_app.editingConfig.mappings[col];
                std::wstringstream alias_ss;
                for (size_t i = 0; i < aliases.size(); ++i) {
                    alias_ss << utf8_to_wstring(aliases[i]);
                    if (i + 1 < aliases.size()) alias_ss << L", ";
                }
                SetWindowTextW(g_app.hMapEditAliases, alias_ss.str().c_str());
                SetWindowTextW(g_app.hMapEditMoveTo, std::to_wstring(curSel + 1).c_str());
            }
            return 0;
        }

        // Manage Mappings: Add New Column
        if (id == ID_MAP_BTN_ADD_COL && code == BN_CLICKED) {
            save_current_alias_box();
            int len = GetWindowTextLengthW(g_app.hMapEditNewCol);
            if (len > 0) {
                std::vector<wchar_t> buf(len + 1, 0);
                GetWindowTextW(g_app.hMapEditNewCol, buf.data(), len + 1);
                std::string newCol = sl::CSVReader::trim(wstring_to_utf8(buf.data()));
                if (!newCol.empty()) {
                    auto it = std::find(g_app.editingConfig.target_headers.begin(), g_app.editingConfig.target_headers.end(), newCol);
                    if (it == g_app.editingConfig.target_headers.end()) {
                        g_app.editingConfig.target_headers.push_back(newCol);
                        if (g_app.editingConfig.mappings.find(newCol) == g_app.editingConfig.mappings.end()) {
                            g_app.editingConfig.mappings[newCol] = {};
                        }
                        SetWindowTextW(g_app.hMapEditNewCol, L"");
                        refresh_manage_columns_list((int)g_app.editingConfig.target_headers.size() - 1);
                    } else {
                        MessageBoxW(hWnd, L"A column with this name already exists.", L"SLMAN", MB_OK | MB_ICONWARNING);
                    }
                }
            }
            return 0;
        }

        // Manage Mappings: Delete Selected Column
        if (id == ID_MAP_BTN_DEL_COL && code == BN_CLICKED) {
            int curSel = (int)SendMessageW(g_app.hMapListColumns, LB_GETCURSEL, 0, 0);
            if (curSel >= 0 && curSel < (int)g_app.editingConfig.target_headers.size()) {
                std::string col = g_app.editingConfig.target_headers[curSel];
                g_app.editingConfig.target_headers.erase(g_app.editingConfig.target_headers.begin() + curSel);
                g_app.editingConfig.mappings.erase(col);
                refresh_manage_columns_list(curSel > 0 ? curSel - 1 : 0);
            }
            return 0;
        }

        // Manage Mappings: Move Up
        if (id == ID_MAP_BTN_MOVE_UP && code == BN_CLICKED) {
            save_current_alias_box();
            int curSel = (int)SendMessageW(g_app.hMapListColumns, LB_GETCURSEL, 0, 0);
            if (curSel > 0 && curSel < (int)g_app.editingConfig.target_headers.size()) {
                std::swap(g_app.editingConfig.target_headers[curSel], g_app.editingConfig.target_headers[curSel - 1]);
                refresh_manage_columns_list(curSel - 1);
            }
            return 0;
        }

        // Manage Mappings: Move Down
        if (id == ID_MAP_BTN_MOVE_DOWN && code == BN_CLICKED) {
            save_current_alias_box();
            int curSel = (int)SendMessageW(g_app.hMapListColumns, LB_GETCURSEL, 0, 0);
            if (curSel >= 0 && curSel + 1 < (int)g_app.editingConfig.target_headers.size()) {
                std::swap(g_app.editingConfig.target_headers[curSel], g_app.editingConfig.target_headers[curSel + 1]);
                refresh_manage_columns_list(curSel + 1);
            }
            return 0;
        }

        // Manage Mappings: Move to Specific Position #
        if (id == ID_MAP_BTN_MOVE_TO && code == BN_CLICKED) {
            save_current_alias_box();
            int curSel = (int)SendMessageW(g_app.hMapListColumns, LB_GETCURSEL, 0, 0);
            if (curSel >= 0 && curSel < (int)g_app.editingConfig.target_headers.size()) {
                wchar_t buf[32] = { 0 };
                GetWindowTextW(g_app.hMapEditMoveTo, buf, 32);
                int targetPos = 1;
                try {
                    targetPos = std::stoi(wstring_to_utf8(buf));
                } catch (...) { targetPos = 1; }

                int targetIdx = targetPos - 1;
                if (targetIdx < 0) targetIdx = 0;
                if (targetIdx >= (int)g_app.editingConfig.target_headers.size()) {
                    targetIdx = (int)g_app.editingConfig.target_headers.size() - 1;
                }

                if (targetIdx != curSel) {
                    std::string col = g_app.editingConfig.target_headers[curSel];
                    g_app.editingConfig.target_headers.erase(g_app.editingConfig.target_headers.begin() + curSel);
                    g_app.editingConfig.target_headers.insert(g_app.editingConfig.target_headers.begin() + targetIdx, col);
                    refresh_manage_columns_list(targetIdx);
                }
            }
            return 0;
        }

        // Manage Mappings: Save to JSON File
        if (id == ID_MAP_BTN_SAVE && code == BN_CLICKED) {
            save_current_alias_box();
            if (sl::ConfigManager::save_to_file(g_app.editingConfig, "mapping_config.json")) {
                g_app.config = g_app.editingConfig;
                append_log("Saved updated schema mappings (" + std::to_string(g_app.config.target_headers.size()) + " columns) to mapping_config.json");
                SetWindowTextW(g_app.hStaticStatus, L"Status: Schema mappings saved successfully!");
                MessageBeep(MB_ICONASTERISK);
            } else {
                MessageBoxW(hWnd, L"Failed to save mapping_config.json", L"SLMAN Error", MB_OK | MB_ICONERROR);
            }
            return 0;
        }

        // Manage Mappings: Reset to Defaults
        if (id == ID_MAP_BTN_RESET && code == BN_CLICKED) {
            int resp = MessageBoxW(hWnd, L"Are you sure you want to reset all columns and aliases to default technical schema?", L"SLMAN Reset", MB_YESNO | MB_ICONQUESTION);
            if (resp == IDYES) {
                g_app.editingConfig = sl::SchemaConfig::get_default();
                sl::ConfigManager::save_to_file(g_app.editingConfig, "mapping_config.json");
                g_app.config = g_app.editingConfig;
                refresh_manage_columns_list(0);
                append_log("Reset schema mappings to factory defaults.");
                SetWindowTextW(g_app.hStaticStatus, L"Status: Reset to defaults.");
            }
            return 0;
        }

        // Manage Mappings: Open File in Explorer / Editor
        if (id == ID_MAP_BTN_OPEN_FILE && code == BN_CLICKED) {
            save_current_alias_box();
            sl::ConfigManager::save_to_file(g_app.editingConfig, "mapping_config.json");
            ShellExecuteW(NULL, L"open", L"mapping_config.json", NULL, NULL, SW_SHOW);
            return 0;
        }

        // Format Mode Switching
        if (id == ID_FMT_MODE_FILE && code == BN_CLICKED) {
            g_app.fmtIsFolder = false;
            InvalidateRect(g_app.hFmtBtnFile, NULL, TRUE);
            InvalidateRect(g_app.hFmtBtnFolder, NULL, TRUE);
            return 0;
        }
        if (id == ID_FMT_MODE_FOLDER && code == BN_CLICKED) {
            g_app.fmtIsFolder = true;
            InvalidateRect(g_app.hFmtBtnFile, NULL, TRUE);
            InvalidateRect(g_app.hFmtBtnFolder, NULL, TRUE);
            return 0;
        }

        // Format Checkbox Toggle
        if (id == ID_FMT_CHK_DROP_EMPTY && code == BN_CLICKED) {
            g_app.fmtDropEmpty = !g_app.fmtDropEmpty;
            InvalidateRect(g_app.hFmtChkDropEmpty, NULL, TRUE);
            return 0;
        }

        // Split Mode Switching
        if (id == ID_SPLIT_MODE_FILE && code == BN_CLICKED) {
            g_app.splitIsFolder = false;
            InvalidateRect(g_app.hSplitBtnFile, NULL, TRUE);
            InvalidateRect(g_app.hSplitBtnFolder, NULL, TRUE);
            return 0;
        }
        if (id == ID_SPLIT_MODE_FOLDER && code == BN_CLICKED) {
            g_app.splitIsFolder = true;
            InvalidateRect(g_app.hSplitBtnFile, NULL, TRUE);
            InvalidateRect(g_app.hSplitBtnFolder, NULL, TRUE);
            return 0;
        }

        // Split Type (Parts vs Rows)
        if (id == ID_SPLIT_TYPE_PARTS && code == BN_CLICKED) {
            g_app.splitIsPartsMode = true;
            InvalidateRect(g_app.hSplitBtnParts, NULL, TRUE);
            InvalidateRect(g_app.hSplitBtnRows, NULL, TRUE);
            return 0;
        }
        if (id == ID_SPLIT_TYPE_ROWS && code == BN_CLICKED) {
            g_app.splitIsPartsMode = false;
            InvalidateRect(g_app.hSplitBtnParts, NULL, TRUE);
            InvalidateRect(g_app.hSplitBtnRows, NULL, TRUE);
            return 0;
        }

        // Split Checkbox Toggle
        if (id == ID_SPLIT_CHK_KEEP_HEADER && code == BN_CLICKED) {
            g_app.splitKeepHeader = !g_app.splitKeepHeader;
            InvalidateRect(g_app.hSplitChkKeepHeader, NULL, TRUE);
            return 0;
        }

        // Clear Log
        if (id == ID_BTN_CLEAR_LOG && code == BN_CLICKED) {
            SetWindowTextW(g_app.hEditLog, L"");
            return 0;
        }

        // Browse Format
        if (id == ID_FMT_BTN_BROWSE && code == BN_CLICKED) {
            if (g_app.fmtIsFolder) {
                fs::path p = sl::DialogUtils::select_folder("Select Folder to Format");
                if (!p.empty()) SetWindowTextW(g_app.hFmtEditPath, p.c_str());
            } else {
                fs::path p = sl::DialogUtils::select_csv_file("Select CSV File to Format");
                if (!p.empty()) SetWindowTextW(g_app.hFmtEditPath, p.c_str());
            }
            return 0;
        }

        // Browse Split
        if (id == ID_SPLIT_BTN_BROWSE && code == BN_CLICKED) {
            if (g_app.splitIsFolder) {
                fs::path p = sl::DialogUtils::select_folder("Select Folder to Split");
                if (!p.empty()) SetWindowTextW(g_app.hSplitEditPath, p.c_str());
            } else {
                fs::path p = sl::DialogUtils::select_csv_file("Select CSV File to Split");
                if (!p.empty()) SetWindowTextW(g_app.hSplitEditPath, p.c_str());
            }
            return 0;
        }

        // Run Format
        if (id == ID_FMT_BTN_RUN && code == BN_CLICKED) {
            if (g_app.isRunning) return 0;

            wchar_t buf[MAX_PATH * 4] = { 0 };
            GetWindowTextW(g_app.hFmtEditPath, buf, MAX_PATH * 4);
            std::string path_str = wstring_to_utf8(buf);

            if (path_str.empty()) {
                MessageBoxW(hWnd, L"Please select a valid CSV file or folder first.", L"SLMAN", MB_OK | MB_ICONWARNING);
                return 0;
            }

            bool drop_empty = g_app.fmtDropEmpty;
            bool is_folder = g_app.fmtIsFolder;

            g_app.isRunning = true;
            set_ui_busy(true);

            std::thread([path_str, is_folder, drop_empty]() {
                run_format_task(path_str, is_folder, drop_empty);
            }).detach();

            return 0;
        }

        // Run Split
        if (id == ID_SPLIT_BTN_RUN && code == BN_CLICKED) {
            if (g_app.isRunning) return 0;

            wchar_t buf[MAX_PATH * 4] = { 0 };
            GetWindowTextW(g_app.hSplitEditPath, buf, MAX_PATH * 4);
            std::string path_str = wstring_to_utf8(buf);

            if (path_str.empty()) {
                MessageBoxW(hWnd, L"Please select a valid CSV file or folder first.", L"SLMAN", MB_OK | MB_ICONWARNING);
                return 0;
            }

            bool is_folder = g_app.splitIsFolder;
            bool is_parts = g_app.splitIsPartsMode;
            bool keep_header = g_app.splitKeepHeader;

            wchar_t count_buf[32] = { 0 };
            GetWindowTextW(g_app.hSplitEditCount, count_buf, 32);
            size_t count_val = 2;
            try {
                count_val = std::stoul(wstring_to_utf8(count_buf));
            } catch (...) { count_val = 2; }
            if (count_val == 0) count_val = 1;

            g_app.isRunning = true;
            set_ui_busy(true);

            std::thread([path_str, is_folder, is_parts, count_val, keep_header]() {
                run_split_task(path_str, is_folder, is_parts, count_val, keep_header);
            }).detach();

            return 0;
        }
        break;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        wchar_t droppedPath[MAX_PATH * 4] = { 0 };
        if (DragQueryFileW(hDrop, 0, droppedPath, MAX_PATH * 4) > 0) {
            fs::path p(droppedPath);
            bool isDir = fs::is_directory(p);

            if (g_app.activeTab == 0) {
                SetWindowTextW(g_app.hFmtEditPath, droppedPath);
                g_app.fmtIsFolder = isDir;
                InvalidateRect(g_app.hFmtBtnFile, NULL, TRUE);
                InvalidateRect(g_app.hFmtBtnFolder, NULL, TRUE);
                append_log("Selected for Format: " + p.filename().string());
            } else if (g_app.activeTab == 1) {
                SetWindowTextW(g_app.hSplitEditPath, droppedPath);
                g_app.splitIsFolder = isDir;
                InvalidateRect(g_app.hSplitBtnFile, NULL, TRUE);
                InvalidateRect(g_app.hSplitBtnFolder, NULL, TRUE);
                append_log("Selected for Split: " + p.filename().string());
            }
        }
        DragFinish(hDrop);
        break;
    }

    case WM_APP_LOG: {
        std::string* pMsg = (std::string*)lParam;
        if (pMsg) {
            append_log(*pMsg);
            delete pMsg;
        }
        break;
    }

    case WM_APP_PROGRESS: {
        g_app.progressCurrent = (int)wParam;
        g_app.progressTotal = (int)lParam;
        RECT rcProg = { 30, 520, 750, 526 };
        InvalidateRect(hWnd, &rcProg, FALSE);
        break;
    }

    case WM_APP_COMPLETED: {
        bool success = (wParam == 1);
        g_app.isRunning = false;
        set_ui_busy(false);
        if (success) {
            SetWindowTextW(g_app.hStaticStatus, L"Status: Finished (Saved to 'format done/' or 'split done/')");
            MessageBeep(MB_ICONASTERISK);
        } else {
            SetWindowTextW(g_app.hStaticStatus, L"Status: Completed with errors");
            MessageBeep(MB_ICONEXCLAMATION);
        }
        break;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        HWND hwndCtrl = (HWND)lParam;
        if (hwndCtrl == g_app.hEditLog) {
            SetTextColor(hdc, Theme::LOG_TXT);
            SetBkColor(hdc, Theme::LOG_BG);
            return (LRESULT)g_app.hBrushLog;
        }
        SetTextColor(hdc, Theme::TEXT_WHITE);
        SetBkColor(hdc, Theme::INPUT_DARK);
        return (LRESULT)g_app.hBrushInput;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hwndCtrl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        if (hwndCtrl == g_app.hLblFmt || hwndCtrl == g_app.hLblSplit || hwndCtrl == g_app.hLblMapCols || hwndCtrl == g_app.hLblMapAliases) {
            SetTextColor(hdc, Theme::TEXT_WHITE);
            return (LRESULT)g_app.hBrushPanel;
        }
        if (hwndCtrl == g_app.hTitle || hwndCtrl == g_app.hStaticStatus) {
            SetTextColor(hdc, Theme::TEXT_WHITE);
            return (LRESULT)g_app.hBrushBg;
        }
        if (hwndCtrl == g_app.hSub) {
            SetTextColor(hdc, Theme::TEXT_MUTED);
            return (LRESULT)g_app.hBrushBg;
        }
        SetTextColor(hdc, Theme::TEXT_GRAY);
        return (LRESULT)g_app.hBrushBg;
    }

    case WM_DESTROY: {
        if (g_app.hFontTitle) DeleteObject(g_app.hFontTitle);
        if (g_app.hFontSub) DeleteObject(g_app.hFontSub);
        if (g_app.hFontBold) DeleteObject(g_app.hFontBold);
        if (g_app.hFontMedium) DeleteObject(g_app.hFontMedium);
        if (g_app.hFontAction) DeleteObject(g_app.hFontAction);
        if (g_app.hFontLog) DeleteObject(g_app.hFontLog);

        if (g_app.hBrushBg) DeleteObject(g_app.hBrushBg);
        if (g_app.hBrushPanel) DeleteObject(g_app.hBrushPanel);
        if (g_app.hBrushInput) DeleteObject(g_app.hBrushInput);
        if (g_app.hBrushLog) DeleteObject(g_app.hBrushLog);
        if (g_app.hPenBorder) DeleteObject(g_app.hPenBorder);
        if (g_app.hPenInputBorder) DeleteObject(g_app.hPenInputBorder);

        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_app.hInstance = hInstance;

    // Load default config
    g_app.config = sl::ConfigManager::load_or_create_default("mapping_config.json");
    g_app.editingConfig = g_app.config;

    // Init Common Controls
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);

    HBRUSH hClassBg = CreateSolidBrush(Theme::BG_BLACK);
    HICON hAppIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!hAppIcon) hAppIcon = LoadIcon(NULL, IDI_APPLICATION);
    HICON hAppIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!hAppIconSm) hAppIconSm = hAppIcon;

    // Register Window Class
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = hClassBg;
    wc.lpszClassName = L"SLMAN_Strict_Monotone_Class";
    wc.hIcon = hAppIcon;
    wc.hIconSm = hAppIconSm;

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    int winWidth = 790;
    int winHeight = 780;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - winWidth) / 2;
    int posY = (screenHeight - winHeight) / 2;

    g_app.hWndMain = CreateWindowExW(
        0,
        L"SLMAN_Strict_Monotone_Class",
        L"SearchLeads CSV Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL);

    if (!g_app.hWndMain) return 1;

    SendMessageW(g_app.hWndMain, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
    SendMessageW(g_app.hWndMain, WM_SETICON, ICON_SMALL, (LPARAM)hAppIconSm);

    ShowWindow(g_app.hWndMain, nCmdShow);
    UpdateWindow(g_app.hWndMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(hClassBg);
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return wWinMain(hInstance, NULL, GetCommandLineW(), nCmdShow);
}
