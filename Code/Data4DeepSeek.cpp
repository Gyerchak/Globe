// Data4Deepseek.cpp
// Location: Code/Data4Deepseek.cpp
// Compile (static): g++ -std=c++20 -static Code/Data4Deepseek.cpp -o Data4Deepseek.exe
// Double‑click Data4Deepseek.exe, or run: ./Data4Deepseek.exe [root_directory]

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <set>
#include <cstdio>
#include <cstdint>

namespace fs = std::filesystem;

// File extensions considered as text/code files
const std::set<std::string> CODE_EXTENSIONS = {
    ".cpp", ".h", ".hpp", ".c", ".cs", ".py", ".java", ".js", ".ts",
    ".json", ".xml", ".yaml", ".yml", ".txt", ".md", ".csv",
    ".html", ".css", ".scss", ".less",
    ".glsl", ".frag", ".vert", ".shader",
    ".cfg", ".ini", ".toml", ".properties",
    ".cmake", ".makefile", ".mak", ".mk",
    ".bat", ".sh", ".ps1", ".zsh", ".bash",
    ".sql", ".r", ".m", ".swift", ".kt",
    ".php", ".rb", ".go", ".rs", ".lua",
    ".tex", ".bib", ".sty",
    ".gitignore", ".gitattributes", ".editorconfig"};

// Maximum file size for "Old" collections: 2 MB
constexpr uintmax_t OLD_MAX_SIZE = 2ULL * 1024ULL * 1024ULL; // 2 MB

// Returns true if the file extension is in our code/text list
static bool isCodeFile(const fs::path &path)
{
    if (!path.has_extension())
        return false;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return CODE_EXTENSIONS.count(ext) > 0;
}

// Returns a human‑readable size string (e.g., "1.23 KB")
static std::string humanReadableSize(uintmax_t bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unitIndex < 4)
    {
        size /= 1024.0;
        unitIndex++;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", size, units[unitIndex]);
    return buf;
}

// Builds the directory tree with file sizes (bytes and bits).
// If 'skipDirs' is provided, those directory names are completely omitted.
static std::string buildTree(const fs::path &root,
                             const std::set<std::string> &skipDirs = {})
{
    std::string result;

    std::function<void(const fs::path &, const std::string &)> walk =
        [&](const fs::path &dir, const std::string &prefix)
    {
        std::vector<fs::directory_entry> entries;
        for (auto &e : fs::directory_iterator(dir))
        {
            if (e.is_directory())
            {
                std::string dirName = e.path().filename().string();
                if (skipDirs.count(dirName))
                    continue; // skip this entire directory
            }
            entries.push_back(e);
        }

        std::sort(entries.begin(), entries.end(),
                  [](const fs::directory_entry &a, const fs::directory_entry &b)
                  {
            if (a.is_directory() != b.is_directory())
                return a.is_directory() > b.is_directory();
            return a.path().filename().string() < b.path().filename().string(); });

        for (size_t i = 0; i < entries.size(); ++i)
        {
            auto &entry = entries[i];
            bool last = (i == entries.size() - 1);
            std::string connector = last ? "└── " : "├── ";
            std::string extensionPrefix = last ? "    " : "│   ";

            std::string name = entry.path().filename().string();
            if (entry.is_directory())
            {
                name += "/";
                result += prefix + connector + name + "\n";
                walk(entry.path(), prefix + extensionPrefix);
            }
            else
            {
                uintmax_t fileSize = 0;
                try
                {
                    fileSize = fs::file_size(entry.path());
                }
                catch (...)
                {
                    fileSize = 0;
                }
                uintmax_t fileSizeBits = fileSize * 8;
                std::string sizeStr = std::to_string(fileSize) + " B (" + std::to_string(fileSizeBits) + " bits)";
                result += prefix + connector + name + "  [" + sizeStr + "]\n";
            }
        }
    };

    std::string rootName = root.filename().string();
    if (rootName.empty())
        rootName = root.string();
    result += rootName + "/\n";
    walk(root, "");
    return result;
}

// Collects files recursively from 'dir'.
// If 'codeOnly' == true, only files matching CODE_EXTENSIONS are included.
// If 'maxSize' > 0, files with size >= maxSize are skipped.
static std::vector<fs::path> collectFiles(const fs::path &dir,
                                          bool codeOnly = true,
                                          uintmax_t maxSize = 0)
{
    std::vector<fs::path> files;
    if (!fs::exists(dir) || !fs::is_directory(dir))
        return files;

    for (auto &entry : fs::recursive_directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;

        // Optional code‑extension filter
        if (codeOnly && !isCodeFile(entry.path()))
            continue;

        // Optional size filter
        if (maxSize > 0)
        {
            uintmax_t sz = 0;
            try
            {
                sz = fs::file_size(entry.path());
            }
            catch (...)
            {
                sz = 0;
            }
            if (sz >= maxSize)
                continue;
        }

        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());
    return files;
}

// Writes a section header and all file contents to an output stream
static void writeFilesSection(std::ofstream &out,
                              const std::vector<fs::path> &files,
                              const fs::path &baseDir,
                              const std::string &sectionTitle)
{
    out << "============================================================================\n";
    out << " " << sectionTitle << " (" << files.size() << " files)\n";
    out << "============================================================================\n\n";

    for (const auto &path : files)
    {
        fs::path relPath = fs::relative(path, baseDir);
        out << "================================================================================\n";
        out << " FILE: " << relPath.string() << "\n";
        out << "================================================================================\n";

        std::ifstream fin(path);
        if (fin)
        {
            out << fin.rdbuf();
            if (fin.bad())
                out << "\n[ERROR reading file]\n";
        }
        else
        {
            out << "[CANNOT OPEN FILE]\n";
        }
        out << "\n\n";
    }
}

// ----------------------------------------------------------------------
int main(int argc, char *argv[])
{
    fs::path rootDir = fs::current_path();
    if (argc > 1)
    {
        rootDir = argv[1];
        if (!fs::exists(rootDir) || !fs::is_directory(rootDir))
        {
            std::cerr << "Error: " << rootDir << " is not a valid directory.\n";
            std::cout << "Press Enter to exit...";
            std::cin.get();
            return 1;
        }
    }

    // Output directory (4DeepSeek) inside the scanned root
    const std::string outDirName = "4DeepSeek";
    fs::path outDir = rootDir / outDirName;
    std::error_code ec;
    fs::create_directories(outDir, ec);

    std::cout << "Scanning directory: " << fs::absolute(rootDir).string() << "\n";

    // ------------------------------------------------------------------
    // 1. Main directory structure (Dir4Deepseek.txt)
    //    Skip .git and .Old entirely, note their exclusion.
    // ------------------------------------------------------------------
    {
        fs::path dirFile = outDir / "Dir4Deepseek.txt";
        std::ofstream out(dirFile);
        if (!out)
        {
            std::cerr << "Error: Cannot create " << dirFile << "\n";
        }
        else
        {
            out << "============================================================================\n";
            out << " DIRECTORY STRUCTURE (file sizes in bytes & bits)\n";
            out << " Root: " << fs::absolute(rootDir).string() << "\n";
            out << "============================================================================\n";
            out << " Note: .git and .Old directories are excluded from this listing.\n\n";

            // Build tree with .git and .Old skipped
            std::set<std::string> skipDirs = {".git", ".Old"};
            out << buildTree(rootDir, skipDirs) << "\n";
            out.close();
            std::cout << "Created " << dirFile.filename().string() << "\n";
        }
    }

    // ------------------------------------------------------------------
    // 2. OldDir4Deepseek.txt – tree of the .Old/ folder only
    // ------------------------------------------------------------------
    {
        fs::path oldDir = rootDir / ".Old";
        fs::path oldDirFile = outDir / "OldDir4Deepseek.txt";
        std::ofstream out(oldDirFile);
        if (!out)
        {
            std::cerr << "Error: Cannot create " << oldDirFile << "\n";
        }
        else
        {
            if (fs::exists(oldDir) && fs::is_directory(oldDir))
            {
                out << "============================================================================\n";
                out << " OLD DIRECTORY STRUCTURE (file sizes in bytes & bits)\n";
                out << " Root: " << fs::absolute(oldDir).string() << "\n";
                out << "============================================================================\n\n";
                out << buildTree(oldDir) << "\n"; // no skip inside .Old
            }
            else
            {
                out << "[.Old directory does not exist]\n";
            }
            out.close();
            std::cout << "Created " << oldDirFile.filename().string() << "\n";
        }
    }

    // ------------------------------------------------------------------
    // 3. Code4Deepseek.txt – ALL code/text files from the Code/ folder
    // ------------------------------------------------------------------
    {
        fs::path codeDir = rootDir / "Code";
        fs::path codeFile = outDir / "Code4Deepseek.txt";
        std::ofstream out(codeFile);
        if (!out)
        {
            std::cerr << "Error: Cannot create " << codeFile << "\n";
        }
        else
        {
            auto files = collectFiles(codeDir, true, 0); // code only, no size limit
            writeFilesSection(out, files, codeDir, "CONTENTS OF CODE/TEXT FILES FROM Code/");
            out.close();
            std::cout << "Created " << codeFile.filename().string() << " (" << files.size() << " files)\n";
        }
    }

    // ------------------------------------------------------------------
    // 4. GameCode4Deepseek.txt – code/text files from GameCode/
    // ------------------------------------------------------------------
    {
        fs::path gameCodeDir = rootDir / "GameCode";
        fs::path outFile = outDir / "GameCode4Deepseek.txt";
        std::ofstream out(outFile);
        if (!out)
        {
            std::cerr << "Error: Cannot create " << outFile << "\n";
        }
        else
        {
            auto files = collectFiles(gameCodeDir, true, 0);
            writeFilesSection(out, files, gameCodeDir, "CONTENTS OF CODE/TEXT FILES FROM GameCode/");
            out.close();
            std::cout << "Created " << outFile.filename().string() << " (" << files.size() << " files)\n";
        }
    }

    // ------------------------------------------------------------------
    // 5. Dependencies4DeepSeek.txt – code/text files from .dependencies/
    //    (previously SourceCode4Deepseek.txt from SourceCode/)
    // ------------------------------------------------------------------
    {
        fs::path dependenciesDir = rootDir / ".dependencies";    // changed
        fs::path outFile = outDir / "4DeepSeekDependencies.txt"; // changed
        std::ofstream out(outFile);
        if (!out)
        {
            std::cerr << "Error: Cannot create " << outFile << "\n";
        }
        else
        {
            auto files = collectFiles(dependenciesDir, true, 0);
            writeFilesSection(out, files, dependenciesDir,
                              "CONTENTS OF CODE/TEXT FILES FROM .dependencies/"); // updated title
            out.close();
            std::cout << "Created " << outFile.filename().string() << " (" << files.size() << " files)\n";
        }
    }

    // ------------------------------------------------------------------
    // 6. OldCodeX.txt – for each immediate subfolder of .Old/
    //    Include ALL files under 2 MB (no extension filter).
    // ------------------------------------------------------------------
    {
        fs::path oldDir = rootDir / ".Old";
        if (fs::exists(oldDir) && fs::is_directory(oldDir))
        {
            for (const auto &entry : fs::directory_iterator(oldDir))
            {
                if (!entry.is_directory())
                    continue;

                std::string subName = entry.path().filename().string();
                fs::path oldOutFile = outDir / ("OldCode" + subName + ".txt");

                std::ofstream out(oldOutFile);
                if (!out)
                {
                    std::cerr << "Error: Cannot create " << oldOutFile << "\n";
                    continue;
                }

                // Collect all files under 2 MB, no code‑only filter
                auto files = collectFiles(entry.path(), false, OLD_MAX_SIZE);
                std::string title = "CONTENTS OF FILES FROM .Old/" + subName + "/ (size < 2 MB)";
                writeFilesSection(out, files, entry.path(), title);
                out.close();
                std::cout << "Created " << oldOutFile.filename().string() << " (" << files.size() << " files)\n";
            }
        }
        else
        {
            std::cout << "Note: .Old directory not found, skipping OldCode generation.\n";
        }
    }

    // =========================================================================
    // 7. Combine into two files: 4Deepseek.txt and 4DeepSeekOld.txt
    // =========================================================================
    {
        // ---- 4Deepseek.txt (main content, WITHOUT dependencies) ----
        std::vector<fs::path> mainPaths;
        mainPaths.push_back(outDir / "Dir4Deepseek.txt");
        mainPaths.push_back(outDir / "Code4Deepseek.txt");
        mainPaths.push_back(outDir / "GameCode4Deepseek.txt");
        // Dependencies file intentionally excluded – it has its own file now.

        fs::path mainFile = outDir / "4Deepseek.txt";
        std::ofstream mainOut(mainFile, std::ios::binary);
        if (!mainOut)
        {
            std::cerr << "Error: Cannot create " << mainFile << "\n";
        }
        else
        {
            for (const auto &path : mainPaths)
            {
                if (!fs::exists(path))
                {
                    std::cout << "Skipping missing file: " << path.filename().string() << "\n";
                    continue;
                }
                std::ifstream in(path, std::ios::binary);
                if (in)
                {
                    mainOut << in.rdbuf() << "\n";
                }
                else
                {
                    std::cerr << "Warning: Could not open " << path.filename().string() << "\n";
                }
            }
            mainOut.close();
            std::cout << "Created " << mainFile.filename().string() << "\n";
        }

        // ---- 4DeepSeekOld.txt (unchanged) ----
        std::vector<fs::path> oldPaths;
        oldPaths.push_back(outDir / "OldDir4Deepseek.txt");

        fs::path oldDir = rootDir / ".Old";
        if (fs::exists(oldDir) && fs::is_directory(oldDir))
        {
            std::vector<std::string> oldSubs;
            for (const auto &entry : fs::directory_iterator(oldDir))
            {
                if (entry.is_directory())
                    oldSubs.push_back(entry.path().filename().string());
            }
            std::sort(oldSubs.begin(), oldSubs.end());
            for (const auto &sub : oldSubs)
            {
                oldPaths.push_back(outDir / ("OldCode" + sub + ".txt"));
            }
        }

        fs::path oldFile = outDir / "4DeepSeekOld.txt";
        std::ofstream oldOut(oldFile, std::ios::binary);
        if (!oldOut)
        {
            std::cerr << "Error: Cannot create " << oldFile << "\n";
        }
        else
        {
            for (const auto &path : oldPaths)
            {
                if (!fs::exists(path))
                {
                    std::cout << "Skipping missing file: " << path.filename().string() << "\n";
                    continue;
                }
                std::ifstream in(path, std::ios::binary);
                if (in)
                {
                    oldOut << in.rdbuf() << "\n";
                }
                else
                {
                    std::cerr << "Warning: Could not open " << path.filename().string() << "\n";
                }
            }
            oldOut.close();
            std::cout << "Created " << oldFile.filename().string() << "\n";
        }
    }

    std::cout << "\nDone. Press Enter to exit...";
    std::cin.get();
    return 0;
}