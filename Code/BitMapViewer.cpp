// BitMapViewer.cpp  (safe 8192x4096 preview, no memory crash)
// Compile: g++ -std=c++20 Code/BitMapViewer.cpp -o Executables/BitMapViewer.exe -static

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../SourceCode/stb_image_write.h"

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

// ------------------------------------------------------------------
// Config – small output for safety (adjustable)
// ------------------------------------------------------------------
constexpr int SRC_WIDTH = 65536;
constexpr int SRC_HEIGHT = 32768;
constexpr int OUT_WIDTH = 8192;  // change to 65536 for full size (requires 6.4 GB RAM!)
constexpr int OUT_HEIGHT = 4096; // change to 32768 accordingly

// ------------------------------------------------------------------
// Colour ramp – blue only for 0, then green→yellow→orange→red→purple→white
// ------------------------------------------------------------------
struct Color
{
    uint8_t r, g, b;
};

Color heightToColor(uint8_t h)
{
    // Only pure 0 is water → blue
    if (h == 0)
        return {0, 0, 255};

    // For h = 1..255, map to green → yellow → orange → red → purple → white
    float t = (h - 1) / 254.0f; // 0.0 at h=1, 1.0 at h=255

    struct Stop
    {
        float pos;
        uint8_t r, g, b;
    };
    constexpr Stop stops[] = {
        {0.0f, 0, 255, 0},    // green
        {0.2f, 255, 255, 0},  // yellow
        {0.4f, 255, 165, 0},  // orange
        {0.6f, 255, 0, 0},    // red
        {0.8f, 128, 0, 128},  // purple
        {1.0f, 255, 255, 255} // white
    };
    constexpr int n = sizeof(stops) / sizeof(stops[0]);

    // find segment
    int i = 0;
    while (i < n - 1 && t > stops[i + 1].pos)
        ++i;
    float segLen = stops[i + 1].pos - stops[i].pos;
    float local = (t - stops[i].pos) / segLen;
    if (local < 0)
        local = 0;
    if (local > 1)
        local = 1;

    return {
        static_cast<uint8_t>(stops[i].r + (stops[i + 1].r - stops[i].r) * local),
        static_cast<uint8_t>(stops[i].g + (stops[i + 1].g - stops[i].g) * local),
        static_cast<uint8_t>(stops[i].b + (stops[i + 1].b - stops[i].b) * local)};
}

// ------------------------------------------------------------------
// Load raw 8-bit heightmap (chunked)
// ------------------------------------------------------------------
std::vector<uint8_t> loadHeightMap(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        std::cerr << "Cannot open " << path << '\n';
        return {};
    }
    constexpr size_t total = static_cast<size_t>(SRC_WIDTH) * SRC_HEIGHT;
    std::vector<uint8_t> data(total);

    constexpr size_t CHUNK = 1ULL << 30; // 1 GiB
    size_t remaining = total;
    size_t offset = 0;
    while (remaining > 0)
    {
        size_t chunkSize = std::min(CHUNK, remaining);
        f.read(reinterpret_cast<char *>(data.data() + offset), chunkSize);
        if (!f)
        {
            std::cerr << "Error reading file (read " << offset << " of " << total << " bytes)\n";
            return {};
        }
        offset += chunkSize;
        remaining -= chunkSize;
    }
    return data;
}

// ------------------------------------------------------------------
// Generate colour PNG data (downscaled)
// ------------------------------------------------------------------
std::vector<uint8_t> createColorRGB(const std::vector<uint8_t> &src)
{
    std::vector<uint8_t> rgb(static_cast<size_t>(OUT_WIDTH) * OUT_HEIGHT * 3);
    for (int y = 0; y < OUT_HEIGHT; ++y)
    {
        int srcY = static_cast<int>(static_cast<long long>(y) * SRC_HEIGHT / OUT_HEIGHT);
        for (int x = 0; x < OUT_WIDTH; ++x)
        {
            int srcX = static_cast<int>(static_cast<long long>(x) * SRC_WIDTH / OUT_WIDTH);
            size_t srcIdx = static_cast<size_t>(srcY) * SRC_WIDTH + srcX;
            uint8_t h = src[srcIdx];
            Color c = heightToColor(h);
            size_t dst = (static_cast<size_t>(y) * OUT_WIDTH + x) * 3;
            rgb[dst + 0] = c.r;
            rgb[dst + 1] = c.g;
            rgb[dst + 2] = c.b;
        }
    }
    return rgb;
}

// ------------------------------------------------------------------
// Generate grayscale PNG data (downscaled)
// ------------------------------------------------------------------
std::vector<uint8_t> createGrayRGB(const std::vector<uint8_t> &src)
{
    std::vector<uint8_t> rgb(static_cast<size_t>(OUT_WIDTH) * OUT_HEIGHT * 3);
    for (int y = 0; y < OUT_HEIGHT; ++y)
    {
        int srcY = static_cast<int>(static_cast<long long>(y) * SRC_HEIGHT / OUT_HEIGHT);
        for (int x = 0; x < OUT_WIDTH; ++x)
        {
            int srcX = static_cast<int>(static_cast<long long>(x) * SRC_WIDTH / OUT_WIDTH);
            size_t srcIdx = static_cast<size_t>(srcY) * SRC_WIDTH + srcX;
            uint8_t h = src[srcIdx];
            size_t dst = (static_cast<size_t>(y) * OUT_WIDTH + x) * 3;
            rgb[dst + 0] = h;
            rgb[dst + 1] = h;
            rgb[dst + 2] = h;
        }
    }
    return rgb;
}

// ------------------------------------------------------------------
// Save PNG
// ------------------------------------------------------------------
bool savePNG(const std::string &filename, const std::vector<uint8_t> &rgb, int w, int h)
{
    if (!stbi_write_png(filename.c_str(), w, h, 3, rgb.data(), w * 3))
    {
        std::cerr << "Failed to write PNG: " << filename << '\n';
        return false;
    }
    std::cout << "Saved " << filename << " (" << w << 'x' << h << ")\n";
    return true;
}

// ------------------------------------------------------------------
// main
// ------------------------------------------------------------------
int main()
{
    // Locate project root
    fs::path exeDir = fs::current_path();
    if (exeDir.filename() == "Executables")
        exeDir = exeDir.parent_path();

    std::string inputPath = (exeDir / "Input" / "HeightMap.bin").string();
    std::cout << "Loading heightmap: " << inputPath << '\n';
    auto raw = loadHeightMap(inputPath);
    if (raw.empty())
    {
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }

    fs::path outputDir = exeDir / "Output";
    fs::create_directories(outputDir);

    // Grayscale PNG
    std::cout << "Creating grayscale PNG (" << OUT_WIDTH << "x" << OUT_HEIGHT << ")...\n";
    auto grayRGB = createGrayRGB(raw);
    if (!savePNG((outputDir / "GrayHeight.png").string(), grayRGB, OUT_WIDTH, OUT_HEIGHT))
    {
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }
    grayRGB.clear();

    // Colour PNG
    std::cout << "Creating colour PNG...\n";
    auto colorRGB = createColorRGB(raw);
    if (!savePNG((outputDir / "ColorHeight.png").string(), colorRGB, OUT_WIDTH, OUT_HEIGHT))
    {
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "\nDone! PNGs saved in " << outputDir.string() << "\n";
    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}