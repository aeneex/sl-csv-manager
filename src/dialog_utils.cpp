#include "dialog_utils.hpp"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <commdlg.h>
#endif

namespace sl {

#if defined(_WIN32) || defined(_WIN64)

namespace {
    struct ComInit {
        HRESULT hr;
        ComInit() {
            hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        }
        ~ComInit() {
            if (SUCCEEDED(hr)) {
                CoUninitialize();
            }
        }
    };

    std::wstring to_wstring(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
}

std::filesystem::path DialogUtils::select_csv_file(const std::string& title) {
    ComInit com;
    IFileOpenDialog* pFileOpen = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, 
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr) && pFileOpen) {
        // Set title
        std::wstring wTitle = to_wstring(title);
        pFileOpen->SetTitle(wTitle.c_str());

        // Set filters
        COMDLG_FILTERSPEC rgSpec[] = {
            { L"CSV Files (*.csv)", L"*.csv" },
            { L"All Files (*.*)", L"*.*" }
        };
        pFileOpen->SetFileTypes(2, rgSpec);
        pFileOpen->SetFileTypeIndex(1);

        // Show dialog
        hr = pFileOpen->Show(NULL);

        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr) && pItem) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr) && pszFilePath) {
                    std::filesystem::path selected_path(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                    pItem->Release();
                    pFileOpen->Release();
                    return selected_path;
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
        return {};
    }

    // Fallback to GetOpenFileNameW
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    std::wstring wTitle = to_wstring(title);
    ofn.lpstrTitle = wTitle.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return std::filesystem::path(szFile);
    }

    return {};
}

std::filesystem::path DialogUtils::select_folder(const std::string& title) {
    ComInit com;
    IFileOpenDialog* pFileOpen = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, 
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr) && pFileOpen) {
        std::wstring wTitle = to_wstring(title);
        pFileOpen->SetTitle(wTitle.c_str());

        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }

        hr = pFileOpen->Show(NULL);

        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr) && pItem) {
                PWSTR pszFolderPath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
                if (SUCCEEDED(hr) && pszFolderPath) {
                    std::filesystem::path selected_path(pszFolderPath);
                    CoTaskMemFree(pszFolderPath);
                    pItem->Release();
                    pFileOpen->Release();
                    return selected_path;
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
        return {};
    }

    // Fallback to SHBrowseForFolderW
    std::wstring wTitle = to_wstring(title);
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.lpszTitle = wTitle.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != 0) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::filesystem::path(path);
        }
        CoTaskMemFree(pidl);
    }

    return {};
}

#else

// Non-Windows fallback
std::filesystem::path DialogUtils::select_csv_file(const std::string& title) {
    std::cout << title << "\nEnter path: ";
    std::string p;
    std::getline(std::cin, p);
    return std::filesystem::path(p);
}

std::filesystem::path DialogUtils::select_folder(const std::string& title) {
    std::cout << title << "\nEnter folder path: ";
    std::string p;
    std::getline(std::cin, p);
    return std::filesystem::path(p);
}

#endif

} // namespace sl
