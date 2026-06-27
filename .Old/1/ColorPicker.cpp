/**
 * ColorPicker – extract & snap unique colours from PNG palettes
 * ==============================================================
 * Input folder:   GameFiles/Palletes/*.png
 * Output:         GameFiles/Palletes/*.txt  (same base name)
 *
 * Each pixel's RGB channel is snapped to the nearest value in
 * {0, 36, 72, 144, 180, 216, 252}.  Only unique colours are saved.
 * Pure white (255;255;255) is ignored entirely.
 *
 * Compile (any shell, stb_image.h in same folder):
 *   g++ -std=c++20 -O2 -static -o ColorPicker.exe ColorPicker.cpp
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdint>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

// Allowed palette
constexpr int ALLOWED[] = {0, 36, 72, 144, 180, 216, 252};
constexpr int NUM_ALLOWED = sizeof(ALLOWED) / sizeof(ALLOWED[0]);

// Snap a single 0‑255 channel to the nearest allowed value
int snap_to_allowed(int val)
{
    int best = ALLOWED[0];
    int bestDist = std::abs(val - ALLOWED[0]);
    for (int i = 1; i < NUM_ALLOWED; ++i)
    {
        int d = std::abs(val - ALLOWED[i]);
        if (d < bestDist)
        {
            bestDist = d;
            best = ALLOWED[i];
        }
    }
    return best;
}

int main()
{
    const std::string inputDir = "GameFiles/Palletes";

    if (!std::filesystem::exists(inputDir))
    {
        std::cerr << "Folder " << inputDir << " does not exist.\n";
        return 1;
    }

    int filesProcessed = 0;

    for (const auto &entry : std::filesystem::directory_iterator(inputDir))
    {
        if (entry.path().extension() != ".png")
            continue;

        std::string pngPath = entry.path().string();
        std::cout << "Processing " << pngPath << " ... ";

        int w, h, channels;
        unsigned char *data = stbi_load(pngPath.c_str(), &w, &h, &channels, 4); // force RGBA
        if (!data)
        {
            std::cerr << "FAILED to load.\n";
            continue;
        }

        // Collect unique snapped colours
        std::set<std::tuple<int, int, int>> uniqueColors;
        for (int i = 0; i < w * h; ++i)
        {
            int r = data[i * 4 + 0];
            int g = data[i * 4 + 1];
            int b = data[i * 4 + 2];

            // Skip pure white (255,255,255)
            if (r == 255 && g == 255 && b == 255)
                continue;

            // snap each channel
            r = snap_to_allowed(r);
            g = snap_to_allowed(g);
            b = snap_to_allowed(b);

            uniqueColors.emplace(r, g, b);
        }

        stbi_image_free(data);

        // Write to .txt file (same name, .txt extension)
        std::filesystem::path txtPath = entry.path();
        txtPath.replace_extension(".txt");
        std::ofstream out(txtPath);
        if (!out)
        {
            std::cerr << "FAILED to write output.\n";
            continue;
        }

        for (const auto &[r, g, b] : uniqueColors)
        {
            out << r << ";" << g << ";" << b << "\n";
        }
        out.close();

        std::cout << uniqueColors.size() << " unique colours saved to "
                  << txtPath.filename().string() << "\n";
        ++filesProcessed;
    }

    std::cout << "\nDone. Processed " << filesProcessed << " PNG files.\n";
    std::cout << "Press Enter to exit...\n";
    std::cin.get();
    return 0;
}