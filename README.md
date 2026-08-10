# SLMAN - CSV Formatter & Splitter (C++)

A lightweight, high-performance C++ tool featuring **both a modern dark-monotone GUI (`slman_GUI.exe`) and an interactive CLI (`slman.exe`)**. It transforms arbitrary CSV column schemas into a standardized 35-column technical schema, prunes empty columns, and splits CSV files into equal parts or by maximum row counts.

---

## 🖥️ Graphical Interface (`slman_GUI.exe`)

`slman_GUI.exe` is a native Windows Win32 application (~830 KB, ~5-10 MB RAM, 0% idle CPU) styled in a sleek, flat dark-monotone aesthetic:

- **Format CSV Tab**:
  - Format single CSV files or entire folders in bulk.
  - Automatically prunes completely empty columns.
  - Saves formatted outputs into dedicated `format done/` subfolders without touching original files.
- **Split CSV Tab**:
  - Split CSVs into $N$ equal parts or by maximum rows per file.
  - Toggle to keep or omit headers across split chunks.
  - Saves split chunks (`filename - 1.csv`, `filename - 2.csv`, ...) directly into `split done/`.
- **Manage Mappings Tab**:
  - **Visual Schema Editor**: View all target columns in exact output order.
  - **Add & Delete Columns**: Create new target columns or remove unwanted ones.
  - **Reorder & Direct "Move to Position #"**: Move any column up/down or jump directly to any specific position rank (e.g. move column 35 directly to position 1).
  - **Edit Aliases**: Easily add raw header aliases for any column using commas or newlines.
  - **Save & Sync**: Save changes straight to `mapping_config.json` with instant updates for both GUI and CLI.
- **Drag & Drop**: Drag CSV files or directories directly onto the window anytime.
- **Live Activity Log & Progress**: Real-time progress bar and log console with detailed timestamps.
- **Non-blocking Engine**: Multithreaded background processing ensures the UI never freezes, even on multi-gigabyte datasets.

---

## ⚡ Shared Architecture

Both the CLI and GUI share the exact same core backend engine:

```
                  ┌──────────────────────────────────────────────┐
                  │              SHARED CORE ENGINE              │
                  ├──────────────────────────────────────────────┤
                  │ • config_manager.cpp / .hpp                  │
                  │ • schema_transformer.cpp / .hpp              │
                  │ • csv_splitter.cpp / .hpp                    │
                  │ • csv_engine.cpp / .hpp                      │
                  │ • dialog_utils.cpp / .hpp                    │
                  └──────────────────────┬───────────────────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    ▼                                         ▼
         ┌─────────────────────┐                   ┌─────────────────────┐
         │      slman.exe      │                   │    slman_GUI.exe    │
         │  (Terminal / CLI)   │                   │    (Windows GUI)    │
         └─────────────────────┘                   └─────────────────────┘
```

Any update to schema mappings or core algorithms automatically takes effect across both tools.

---

## 📦 Installation & Deployment

Running `build.bat` or `install.bat` automatically deploys a standalone installation into your Windows Programs directory:

```
%LOCALAPPDATA%\Programs\slman\
```
*(e.g., `C:\Users\<User>\AppData\Local\Programs\slman`)*

### Installed Components:
- `slman_GUI.exe` — Standalone GUI application
- `slman.exe` — Standalone CLI / Console application
- `mapping_config.json` — Schema definition and column alias mappings
- `uninstall.bat` — Independent uninstaller
- `slman.bat` — Quick terminal launcher

### Automatic Setup:
1. **Desktop Shortcut**: Creates a `SLMAN GUI` shortcut.
2. **Start Menu Entries**: Creates `SLMAN GUI`, `SLMAN CLI`, and `Uninstall slman`.
3. **User PATH Integration**: Registers the folder to your `PATH` so you can run `slman` from any terminal or PowerShell prompt.

To completely remove the installation at any time, run `uninstall.bat`.

---

## 🚀 Quick Start

### 1. Launching the GUI
Double-click `slman_GUI.exe` or open **SLMAN GUI** from your Desktop.

### 2. Interactive CLI Mode (Menu-Driven)
Run `slman` from any terminal, or launch `slman.bat`:
```
Select an operation:

  [1] Format single CSV file to Technical Schema  --> Opens File Explorer window
  [2] Split single CSV file into N parts          --> Opens File Explorer window
  [3] Bulk: Format all CSV files in a folder      --> Opens Folder Explorer window
  [4] Bulk: Split all CSV files in a folder       --> Opens Folder Explorer window
  [5] View Target Schema & Mappings
  [6] Exit
```

### 3. CLI Command-Line Arguments

#### Format CSV Schema:
```bash
# Format input.csv, prune empty columns, and save into "format done/input.csv"
slman format input.csv

# Format with custom output path
slman format input.csv -o output.csv

# Format with custom config file
slman format input.csv -c custom_mapping.json
```

#### Split CSV:
```bash
# Split into 2 parts and save into "split done/" folder
slman split input.csv --parts 2

# Split into 4 parts without repeating headers in parts 2..N
slman split input.csv --parts 4 --no-headers

# Split by max rows per file (e.g. 5000 rows each)
slman split input.csv --max-rows 5000
```

#### Bulk Folder Processing:
```bash
# Format all CSV files in a folder (saves into <folder>/format done/<filename>)
slman bulk "C:\data\leads" --format

# Split all CSV files in a folder into 3 parts (saves into <folder>/split done/)
slman bulk "C:\data\leads" --split 3
```

---

## 📋 Default 35-Column Technical Schema

```
 1. id                               19. country
 2. index                            20. state
 3. first_name                       21. city
 4. last_name                        22. organization_name
 5. personal_email                   23. organization_linkedin_url
 6. email                            24. organization_linkedin_description
 7. email_status                     25. organization_overview
 8. valid_mobile_number              26. organization_founded_year
 9. name                             27. estimated_num_employees
10. headline                         28. organization_primary_domain
11. title                            29. organization_country
12. linkedin_url                     30. organization_state
13. person_linkedin_profile_summary  31. organization_city
14. skills                           32. organization_postal_code
15. department                       33. raw_address
16. sub_departments                  34. industry
17. functions                        35. keywords
18. seniority
```

---

## 🛠️ Building from Source

### Prerequisites:
- CMake 3.20+
- MinGW-w64 (GCC 11+) or MSVC

### Build Command:
```bat
build.bat
```
*(Compiles both `slman.exe` and `slman_GUI.exe`, verifies linking, and automatically updates `%LOCALAPPDATA%\Programs\slman`).*
