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

namespace fs = std::filesystem;

// File extensions considered as text/code files to include in Code4Deepseek.txt
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

// Builds the directory tree with file sizes (bytes and bits) and skips folders named "Old"
static std::string buildTree(const fs::path &root)
{
    std::string result;

    std::function<void(const fs::path &, const std::string &)> walk =
        [&](const fs::path &dir, const std::string &prefix)
    {
        std::vector<fs::directory_entry> entries;
        for (auto &e : fs::directory_iterator(dir))
        {
            // Skip any directory named "Old"
            if (e.is_directory() && e.path().filename() == "Old")
            {
                continue;
            }
            entries.push_back(e);
        }

        std::sort(entries.begin(), entries.end(), [](const fs::directory_entry &a, const fs::directory_entry &b)
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

// Collects all code/text files recursively, skipping "Old" directories
static std::vector<fs::path> collectCodeFiles(const fs::path &root)
{
    std::vector<fs::path> files;
    for (auto &entry : fs::recursive_directory_iterator(root))
    {
        bool skip = false;
        for (auto &comp : entry.path())
        {
            if (comp == "Old")
            {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;

        if (entry.is_regular_file() && isCodeFile(entry.path()))
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

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

    // Output directory and files
    const std::string outDirName = "4DeepSeek";
    fs::path outDir = rootDir / outDirName;
    std::error_code ec;
    fs::create_directories(outDir, ec); // create 4DeepSeek if not exists

    const std::string dirFile = (outDir / "Dir4Deepseek.txt").string();
    const std::string codeFile = (outDir / "Code4Deepseek.txt").string();

    std::cout << "Scanning directory: " << fs::absolute(rootDir).string() << "\n";

    // 1. Directory structure with sizes
    std::ofstream outDirFile(dirFile);
    if (!outDirFile)
    {
        std::cerr << "Error: Cannot create " << dirFile << "\n";
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }
    outDirFile << "============================================================================\n";
    outDirFile << " DIRECTORY STRUCTURE (file sizes in bytes & bits)\n";
    outDirFile << " Root: " << fs::absolute(rootDir).string() << "\n";
    outDirFile << "============================================================================\n\n";
    outDirFile << buildTree(rootDir) << "\n";
    outDirFile.close();
    std::cout << "Created " << dirFile << "\n";

    // 2. Code/text file contents
    std::vector<fs::path> codeFiles = collectCodeFiles(rootDir);
    std::ofstream outCodeFile(codeFile);
    if (!outCodeFile)
    {
        std::cerr << "Error: Cannot create " << codeFile << "\n";
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }
    outCodeFile << "============================================================================\n";
    outCodeFile << " CONTENTS OF CODE/TEXT FILES (" << codeFiles.size() << " files)\n";
    outCodeFile << "============================================================================\n\n";

    for (const auto &path : codeFiles)
    {
        fs::path relPath = fs::relative(path, rootDir);
        outCodeFile << "================================================================================\n";
        outCodeFile << " FILE: " << relPath.string() << "\n";
        outCodeFile << "================================================================================\n";

        std::ifstream fin(path);
        if (fin)
        {
            outCodeFile << fin.rdbuf();
            if (fin.bad())
                outCodeFile << "\n[ERROR reading file]\n";
        }
        else
        {
            outCodeFile << "[CANNOT OPEN FILE]\n";
        }
        outCodeFile << "\n\n";
    }
    outCodeFile.close();
    std::cout << "Created " << codeFile << " (" << codeFiles.size() << " files)\n";

    std::cout << "\nDone. Press Enter to exit...";
    std::cin.get();
    return 0;
}