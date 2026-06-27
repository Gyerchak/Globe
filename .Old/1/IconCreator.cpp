/**
 * IconCreator – PNG to 9‑bit packed icon atlas (128×128 icons)
 * =============================================================
 * Reads all .png files from GameFiles/Icons/.  Every icon is
 * resized to 128×128 pixels, quantised to 3‑bit colour, and
 * packed onto an 8192×8192 canvas.  IconsPacked.bin is written
 * to GameData/Icons/.
 *
 * Atlas capacity: (8192/128)^2 = 4096 icons.
 *
 * Compile (any shell, no extra libraries):
 *   g++ -std=c++20 -O2 -static -o IconCreator.exe IconCreator.cpp
 */

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using u16 = uint16_t;
using u32 = uint32_t;
using u8 = uint8_t;

// ------------------------------------------------------------------
//  Bilinear resizing of an RGBA image
// ------------------------------------------------------------------
static std::vector<u8> resizeImage(const u8 *srcData, int srcW, int srcH,
                                   int dstW, int dstH)
{
    std::vector<u8> dst(dstW * dstH * 4);
    float scaleX = (float)(srcW - 1) / (dstW - 1);
    float scaleY = (float)(srcH - 1) / (dstH - 1);

    for (int dy = 0; dy < dstH; ++dy)
    {
        float sy = dy * scaleY;
        int y0 = (int)sy;
        int y1 = (std::min)(y0 + 1, srcH - 1);
        float fy = sy - y0;

        for (int dx = 0; dx < dstW; ++dx)
        {
            float sx = dx * scaleX;
            int x0 = (int)sx;
            int x1 = (std::min)(x0 + 1, srcW - 1);
            float fx = sx - x0;

            for (int c = 0; c < 4; ++c)
            {
                float v00 = srcData[(y0 * srcW + x0) * 4 + c];
                float v10 = srcData[(y0 * srcW + x1) * 4 + c];
                float v01 = srcData[(y1 * srcW + x0) * 4 + c];
                float v11 = srcData[(y1 * srcW + x1) * 4 + c];

                float v0 = v00 + (v10 - v00) * fx;
                float v1 = v01 + (v11 - v01) * fx;
                float v = v0 + (v1 - v0) * fy;

                dst[(dy * dstW + dx) * 4 + c] = (u8)(v + 0.5f); // round
            }
        }
    }
    return dst;
}

// ------------------------------------------------------------------
//  Quantise 8‑bit (0‑255) → 3‑bit (0‑7)
// ------------------------------------------------------------------
inline u16 quantise8to3(u32 val)
{
    return (u16)(val / 36); // 0..36 → 0, 36..72 → 1, … 216..252 → 7
}

int main()
{
    const std::string inputDir = "GameFiles/Icons";
    const std::string outputPath = "GameData/Icons/IconsPacked.bin";

    std::filesystem::create_directories("GameData/Icons");

    // Collect PNG files
    std::vector<std::string> pngFiles;
    for (auto &entry : std::filesystem::directory_iterator(inputDir))
    {
        if (entry.path().extension() == ".png")
            pngFiles.push_back(entry.path().string());
    }
    if (pngFiles.empty())
    {
        std::cerr << "No PNG files found in " << inputDir << "\n";
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }
    std::sort(pngFiles.begin(), pngFiles.end());

    // -------- Fixed icon size --------
    const int ICON_W = 128;
    const int ICON_H = 128;
    std::printf("All icons will be resized to %d×%d, found %zu icons\n",
                ICON_W, ICON_H, pngFiles.size());

    const int ATLAS_W = 8192, ATLAS_H = 8192;
    const int cols = ATLAS_W / ICON_W;
    const int rows = ATLAS_H / ICON_H;
    if (pngFiles.size() > (size_t)(cols * rows))
    {
        std::printf("Warning: too many icons for atlas (%zu > %d). Extra icons ignored.\n",
                    pngFiles.size(), cols * rows);
    }

    std::vector<u16> atlas(ATLAS_W * ATLAS_H, 0);

    // -------- Process each icon --------
    for (size_t idx = 0; idx < pngFiles.size() && idx < (size_t)(cols * rows); ++idx)
    {
        int w, h, c;
        unsigned char *data = stbi_load(pngFiles[idx].c_str(), &w, &h, &c, 4);
        if (!data)
        {
            std::fprintf(stderr, "Failed to load %s\n", pngFiles[idx].c_str());
            std::printf("\nPress Enter to exit...\n");
            std::getchar();
            return 2;
        }

        // Resize to fixed 128×128 if needed
        std::vector<u8> resizedData;
        const u8 *pixelData = data;
        if (w != ICON_W || h != ICON_H)
        {
            resizedData = resizeImage(data, w, h, ICON_W, ICON_H);
            pixelData = resizedData.data();
            stbi_image_free(data);
        }

        int row = (int)(idx / cols);
        int col = (int)(idx % cols);
        int baseX = col * ICON_W;
        int baseY = row * ICON_H;

        for (int y = 0; y < ICON_H; ++y)
        {
            for (int x = 0; x < ICON_W; ++x)
            {
                int srcIdx = (y * ICON_W + x) * 4;
                u16 r = quantise8to3(pixelData[srcIdx + 0]);
                u16 g = quantise8to3(pixelData[srcIdx + 1]);
                u16 b = quantise8to3(pixelData[srcIdx + 2]);
                u16 packed = (u16)((r << 6) | (g << 3) | b);
                atlas[(baseY + y) * ATLAS_W + (baseX + x)] = packed;
            }
        }

        if (pixelData == data) // no resizing happened
            stbi_image_free(data);
    }

    // -------- Write output --------
    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        std::perror("Cannot write output");
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 4;
    }
    out.write(reinterpret_cast<const char *>(atlas.data()), atlas.size() * sizeof(u16));
    out.close();
    std::printf("Wrote %zu pixels to %s\n", atlas.size(), outputPath.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}