/**
 * ColorPrinter – MYK mixing of ALL triples from external palette file
 * ===================================================================
 * Reads ALL base colours from GameFiles/Palletes/1SkinPallete.txt
 * (one "R;G;B" per line).  For every combination of 3 distinct colours,
 * mixes them in MYK space (C=0) with weights 0‑6, converts back to RGB,
 * snaps to {0,36,72,144,180,216,252}, and saves the unique results as 64×64 PNGs.
 *
 * Output folder: GameFiles/Npc/7SkinPigments/
 *
 * Compile (any shell, stb_image_write.h in same folder):
 *   g++ -std=c++20 -O2 -static -o ColorPrinter.exe ColorPrinter.cpp
 */

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdint>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

// Allowed palette
constexpr int ALLOWED[] = {0, 36, 72, 144, 180, 216, 252};
constexpr int NUM_ALLOWED = sizeof(ALLOWED) / sizeof(ALLOWED[0]);

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

int clamp255(int x) { return std::max(0, std::min(255, x)); }

// ------------------------------------------------------------------
//  RGB ↔ MYK conversion (Cyan = 0) – identical to early versions
// ------------------------------------------------------------------
struct MYK
{
    double m, y, k;
};

MYK rgb_to_myk(int r, int g, int b)
{
    double k = 255.0 - r;
    double m, y;
    if (r > 0)
    {
        m = 255.0 - (g * 255.0) / r;
        y = 255.0 - (b * 255.0) / r;
    }
    else
    {
        m = 255.0;
        y = 255.0;
    }
    m = clamp255((int)std::round(m));
    y = clamp255((int)std::round(y));
    k = clamp255((int)std::round(k));
    return {m, y, k};
}

std::tuple<int, int, int> myk_to_rgb(double m, double y, double k)
{
    double r = 255.0 - k;
    double g = (255.0 - m) * (255.0 - k) / 255.0;
    double b = (255.0 - y) * (255.0 - k) / 255.0;
    int ri = clamp255((int)std::round(r));
    int gi = clamp255((int)std::round(g));
    int bi = clamp255((int)std::round(b));
    return {ri, gi, bi};
}

// ------------------------------------------------------------------
int main()
{
    // ----- 1. Read ALL base colours from file -----
    const std::string palettePath = "GameFiles/Palletes/1SkinPallete.txt";
    std::ifstream file(palettePath);
    if (!file)
    {
        std::cerr << "Cannot open " << palettePath << "\n";
        return 1;
    }

    std::vector<std::tuple<int, int, int>> baseColors;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        int r, g, b;
        char sep1, sep2;
        if (ss >> r >> sep1 >> g >> sep2 >> b && sep1 == ';' && sep2 == ';')
        {
            baseColors.emplace_back(r, g, b);
        }
    }
    file.close();

    if (baseColors.size() < 3)
    {
        std::cerr << "Need at least 3 base colours, got " << baseColors.size() << "\n";
        return 1;
    }

    std::cout << "Loaded " << baseColors.size() << " base colours.\n";
    std::cout << "Will mix every combination of 3 colours (MYK space, no Cyan).\n";

    // Pre‑convert all base colours to MYK
    std::vector<MYK> baseMYK;
    for (auto [r, g, b] : baseColors)
    {
        baseMYK.push_back(rgb_to_myk(r, g, b));
    }

    // ----- 2. Mix every triple -----
    std::set<std::tuple<int, int, int>> uniqueColors;
    size_t tripleCount = 0;

    for (size_t i = 0; i < baseColors.size(); ++i)
    {
        for (size_t j = i + 1; j < baseColors.size(); ++j)
        {
            for (size_t k = j + 1; k < baseColors.size(); ++k)
            {
                tripleCount++;
                // Weighted mixing for this triple
                for (int w1 = 0; w1 <= 6; ++w1)
                {
                    for (int w2 = 0; w2 <= 6; ++w2)
                    {
                        for (int w3 = 0; w3 <= 6; ++w3)
                        {
                            int sum = w1 + w2 + w3;
                            if (sum == 0)
                                continue;

                            double mixedM = (w1 * baseMYK[i].m + w2 * baseMYK[j].m + w3 * baseMYK[k].m) / sum;
                            double mixedY = (w1 * baseMYK[i].y + w2 * baseMYK[j].y + w3 * baseMYK[k].y) / sum;
                            double mixedK = (w1 * baseMYK[i].k + w2 * baseMYK[j].k + w3 * baseMYK[k].k) / sum;

                            auto [r, g, b] = myk_to_rgb(mixedM, mixedY, mixedK);
                            r = snap_to_allowed(r);
                            g = snap_to_allowed(g);
                            b = snap_to_allowed(b);
                            uniqueColors.emplace(r, g, b);
                        }
                    }
                }
            }
        }
    }

    std::cout << "Processed " << tripleCount << " triples.\n";
    std::cout << "Unique mixed colours: " << uniqueColors.size() << "\n";

    // ----- 3. Write PNGs -----
    std::filesystem::path outDir = "GameFiles/Npc/7SkinPigments";
    std::filesystem::create_directories(outDir);

    constexpr int W = 64, H = 64, CH = 3;
    std::vector<uint8_t> pixels(W * H * CH);

    size_t idx = 0;
    for (const auto &[r, g, b] : uniqueColors)
    {
        for (int p = 0; p < W * H; ++p)
        {
            pixels[p * CH + 0] = (uint8_t)r;
            pixels[p * CH + 1] = (uint8_t)g;
            pixels[p * CH + 2] = (uint8_t)b;
        }
        std::string filename = (outDir / (std::to_string(idx) + ".png")).string();
        if (!stbi_write_png(filename.c_str(), W, H, CH, pixels.data(), W * CH))
        {
            std::cerr << "Failed to write " << filename << "\n";
            return 1;
        }
        ++idx;
    }

    std::cout << "\nAll " << uniqueColors.size() << " PNGs written to "
              << std::filesystem::absolute(outDir) << "\n";
    std::cout << "Press Enter to exit...\n";
    std::cin.get();
    return 0;
}