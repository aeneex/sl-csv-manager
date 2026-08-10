# SearchLeads CSV Manager (`slman`)

A lightweight, high-performance C++ tool featuring **both a modern dark-monotone GUI (`slman_GUI.exe`) and an interactive CLI (`slman.exe`)**. It transforms arbitrary CSV column schemas into a standardized 35-column technical schema, prunes empty columns, and splits CSV files into equal parts or by maximum row counts.

---

## ⚡ Quick 1-Click Installation (No Tools Required)

Precompiled, fully standalone binaries are already included in this repository. **You do not need CMake, Python, or a C++ compiler to install and run the app.**

1. Clone or download this repository.
2. Double-click **`install.bat`**.

That's it! `install.bat` automatically:
- Installs the app to `%LOCALAPPDATA%\Programs\slman\`
- Creates a **SearchLeads CSV Manager** shortcut on your Desktop
- Creates Start Menu shortcuts (**SearchLeads CSV Manager**, **slman CLI**, **Uninstall**)
- Registers `slman` to your User `PATH` so you can use it from any terminal.

---

## 🖥️ Graphical Interface

The GUI is an ultra-lightweight native Windows application (~870 KB, ~5-10 MB RAM, 0% idle CPU) styled in a sleek, flat dark-monotone aesthetic:

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

## 💻 CLI & Interactive Console Usage

You can run `slman` from any terminal or double-click `slman.bat`:

### 1. Interactive Menu Mode
Running `slman` without arguments opens the interactive menu:
```
Select an operation:

  [1] Format single CSV file to Technical Schema  --> Opens File Explorer window
  [2] Split single CSV file into N parts          --> Opens File Explorer window
  [3] Bulk: Format all CSV files in a folder      --> Opens Folder Explorer window
  [4] Bulk: Split all CSV files in a folder       --> Opens Folder Explorer window
  [5] View Target Schema & Mappings
  [6] Exit
```

### 2. Command-Line Arguments

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

## 🛠️ Building from Source (Developers Only)

If you modify the C++ source code in `src/` and wish to recompile the project:

### Prerequisites:
- CMake 3.20+
- MinGW-w64 (GCC 11+) or MSVC

### Build Command:
```bat
build.bat
```
*(Compiles statically linked binaries to `build/` and auto-invokes `install.bat`).*

---

## 🗑️ Uninstallation

To completely remove the application, shortcuts, and PATH registration at any time, run:
```bat
uninstall.bat
```
*(or launch the **Uninstall** shortcut from the Start Menu).*
