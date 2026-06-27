/**
 * NpcIconCreator – Green chroma key + white skin recolouring (updated paths)
 * ===========================================================================
 * Input layers (6 folders under GameFiles/Npc/):
 *   1BodyType
 *   2Jaw
 *   3ChewBones
 *   4ChickBones
 *   5ForeHead
 *   6HeadBack
 *
 * Skin pigments: GameFiles/Npc/7SkinPigments/
 *
 * Green key:   RGB(0,252,0) → treated as fully transparent when compositing,
 *              then filled as opaque green background where nothing else is drawn.
 * Skin mask:   White (255,255,255) → replaced by the current skin pigment colour.
 *
 * Output: GameData/Icons/NpcIconsPacked.bin
 *
 * Compile (any shell, stb_image.h in same folder):
 *   g++ -std=c++20 -O2 -static -o NpcIconCreator.exe NpcIconCreator.cpp
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>

using u16 = uint16_t;
using u32 = uint32_t;
using u8 = uint8_t;

// ------------------------------------------------------------------
//  Bilinear resize (RGBA image)
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

                dst[(dy * dstW + dx) * 4 + c] = (u8)(v + 0.5f);
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
    return (u16)(val / 36); // 0..35→0, 36..71→1, … 216..251→6, 252..255→7
}

// ------------------------------------------------------------------
int main()
{
    // ----- Folder setup (6 layers, Neck removed) -----
    const std::string layerFolders[6] = {
        "GameFiles/Npc/1BodyType",
        "GameFiles/Npc/2Jaw",
        "GameFiles/Npc/3ChewBones",
        "GameFiles/Npc/4ChickBones",
        "GameFiles/Npc/5ForeHead",
        "GameFiles/Npc/6HeadBack"};
    const std::string pigmentDir = "GameFiles/Npc/7SkinPigments";
    const std::string outputPath = "GameData/Icons/NpcIconsPacked.bin";

    // ----- Read skin pigment colours -----
    std::vector<std::tuple<int, int, int>> pigments; // R,G,B 0‑255
    for (auto &entry : std::filesystem::directory_iterator(pigmentDir))
    {
        if (entry.path().extension() == ".png")
        {
            int w, h, c;
            unsigned char *data = stbi_load(entry.path().string().c_str(), &w, &h, &c, 4);
            if (!data)
            {
                std::fprintf(stderr, "Failed to load pigment %s\n", entry.path().string().c_str());
                return 1;
            }
            pigments.emplace_back(data[0], data[1], data[2]);
            stbi_image_free(data);
        }
    }
    if (pigments.empty())
    {
        std::fprintf(stderr, "No skin pigments found in %s\n", pigmentDir.c_str());
        return 1;
    }
    std::cout << "Loaded " << pigments.size() << " skin pigments.\n";

    // ----- Pre‑collect layer file paths and count combinations -----
    std::vector<std::vector<std::string>> layerFiles(6);
    size_t baseCombos = 1;
    for (int f = 0; f < 6; ++f)
    {
        for (auto &entry : std::filesystem::directory_iterator(layerFolders[f]))
        {
            if (entry.path().extension() == ".png")
                layerFiles[f].push_back(entry.path().string());
        }
        if (layerFiles[f].empty())
        {
            std::fprintf(stderr, "No PNG files in %s\n", layerFolders[f].c_str());
            return 1;
        }
        std::sort(layerFiles[f].begin(), layerFiles[f].end());
        baseCombos *= layerFiles[f].size();
    }
    size_t totalIcons = baseCombos * pigments.size();
    std::cout << "Layer file counts: ";
    for (int f = 0; f < 6; ++f)
        std::cout << layerFiles[f].size() << " ";
    std::cout << "\nBase layer combinations: " << baseCombos << "\n";
    std::cout << "Total icons (with skin pigments): " << totalIcons << "\n";

    // ----- Icon size (fixed) -----
    const int iconSize = 64;

    // ----- Compute required atlas dimensions -----
    size_t gridCols = (size_t)std::ceil(std::sqrt((double)totalIcons));
    size_t gridRows = (totalIcons + gridCols - 1) / gridCols;
    size_t sidePixels = std::max(gridCols, gridRows) * iconSize;

    const size_t MAX_ATLAS_SIDE = 16384;
    if (sidePixels > MAX_ATLAS_SIDE)
    {
        std::fprintf(stderr,
                     "\nERROR: Required atlas side (%zu×%zu) exceeds GPU texture limit (%zu).\n"
                     "Reduce the number of icons (fewer variants or fewer pigments).\n",
                     sidePixels, sidePixels, MAX_ATLAS_SIDE);
        return 1;
    }

    size_t pixelCount = sidePixels * sidePixels;
    double cpuRamMB = (pixelCount * sizeof(u16)) / (1024.0 * 1024.0);
    double gpuMemMB = (pixelCount * 4) / (1024.0 * 1024.0);
    double diskMB = (pixelCount * sizeof(u16)) / (1024.0 * 1024.0);
    std::cout << "\nAtlas size: " << sidePixels << "×" << sidePixels << " pixels\n";
    std::cout << "CPU RAM used (atlas buffer): ~" << cpuRamMB << " MB\n";
    std::cout << "GPU memory needed (texture): ~" << gpuMemMB << " MB\n";
    std::cout << "Disk space (output file):   ~" << diskMB << " MB\n";

    // ----- Create atlas buffer -----
    std::vector<u16> atlas(pixelCount, 0);

    // ----- Generate icons -----
    size_t iconIndex = 0;
    std::vector<int> indices(6, 0); // current layer combination
    std::vector<u8> canvas;         // RGBA composite buffer

    while (true)
    {
        // Composite layers for this combination
        canvas.assign(iconSize * iconSize * 4, 0); // transparent

        for (int f = 0; f < 6; ++f)
        {
            int w, h, c;
            unsigned char *data = stbi_load(layerFiles[f][indices[f]].c_str(), &w, &h, &c, 4);
            if (!data)
            {
                std::fprintf(stderr, "Failed to load %s\n", layerFiles[f][indices[f]].c_str());
                return 1;
            }
            std::vector<u8> resized;
            const u8 *srcPixels;
            if (w != iconSize || h != iconSize)
            {
                resized = resizeImage(data, w, h, iconSize, iconSize);
                srcPixels = resized.data();
            }
            else
            {
                resized.assign(data, data + (iconSize * iconSize * 4));
                srcPixels = resized.data();
            }
            stbi_image_free(data);

            // Process this layer: set alpha=0 for green (0,252,0) pixels
            std::vector<u8> layerPixels(iconSize * iconSize * 4);
            for (int i = 0; i < iconSize * iconSize; ++i)
            {
                u8 r = srcPixels[i * 4 + 0];
                u8 g = srcPixels[i * 4 + 1];
                u8 b = srcPixels[i * 4 + 2];
                u8 a = srcPixels[i * 4 + 3];
                if (r == 0 && g == 252 && b == 0)
                {
                    a = 0; // make green fully transparent
                }
                layerPixels[i * 4 + 0] = r;
                layerPixels[i * 4 + 1] = g;
                layerPixels[i * 4 + 2] = b;
                layerPixels[i * 4 + 3] = a;
            }

            // Source‑over composite onto canvas
            for (int i = 0; i < iconSize * iconSize; ++i)
            {
                u8 sr = layerPixels[i * 4 + 0];
                u8 sg = layerPixels[i * 4 + 1];
                u8 sb = layerPixels[i * 4 + 2];
                u8 sa = layerPixels[i * 4 + 3];
                if (sa == 0)
                    continue;

                u8 dr = canvas[i * 4 + 0];
                u8 dg = canvas[i * 4 + 1];
                u8 db = canvas[i * 4 + 2];
                u8 da = canvas[i * 4 + 3];

                float srcA = sa / 255.f;
                float dstA = da / 255.f;
                float outA = srcA + dstA * (1.f - srcA);
                if (outA < 1e-6f)
                {
                    canvas[i * 4 + 0] = canvas[i * 4 + 1] = canvas[i * 4 + 2] = canvas[i * 4 + 3] = 0;
                    continue;
                }
                float outR = (sr * srcA + dr * dstA * (1.f - srcA)) / outA;
                float outG = (sg * srcA + dg * dstA * (1.f - srcA)) / outA;
                float outB = (sb * srcA + db * dstA * (1.f - srcA)) / outA;

                canvas[i * 4 + 0] = (u8)(outR + 0.5f);
                canvas[i * 4 + 1] = (u8)(outG + 0.5f);
                canvas[i * 4 + 2] = (u8)(outB + 0.5f);
                canvas[i * 4 + 3] = (u8)(outA * 255.f + 0.5f);
            }
        }

        // After compositing, fill fully transparent pixels with green
        for (int i = 0; i < iconSize * iconSize; ++i)
        {
            if (canvas[i * 4 + 3] == 0)
            {
                canvas[i * 4 + 0] = 0;
                canvas[i * 4 + 1] = 252;
                canvas[i * 4 + 2] = 0;
                canvas[i * 4 + 3] = 255;
            }
        }

        // Generate each skin pigment variant
        for (const auto &[pigR, pigG, pigB] : pigments)
        {
            std::vector<u8> coloured = canvas; // copy

            // Replace white (255,255,255) with skin pigment
            for (int i = 0; i < iconSize * iconSize; ++i)
            {
                if (coloured[i * 4 + 0] == 255 && coloured[i * 4 + 1] == 255 && coloured[i * 4 + 2] == 255)
                {
                    coloured[i * 4 + 0] = (u8)pigR;
                    coloured[i * 4 + 1] = (u8)pigG;
                    coloured[i * 4 + 2] = (u8)pigB;
                }
            }

            // Place into atlas
            int row = (int)(iconIndex / gridCols);
            int col = (int)(iconIndex % gridCols);
            int baseX = col * iconSize;
            int baseY = row * iconSize;

            for (int y = 0; y < iconSize; ++y)
            {
                for (int x = 0; x < iconSize; ++x)
                {
                    int pi = (y * iconSize + x) * 4;
                    u16 r3 = quantise8to3(coloured[pi + 0]);
                    u16 g3 = quantise8to3(coloured[pi + 1]);
                    u16 b3 = quantise8to3(coloured[pi + 2]);
                    u16 packed = (u16)((r3 << 6) | (g3 << 3) | b3);
                    atlas[(baseY + y) * sidePixels + (baseX + x)] = packed;
                }
            }
            iconIndex++;
        }

        // Advance to next layer combination
        int carry = 5;
        while (carry >= 0)
        {
            indices[carry]++;
            if (indices[carry] < (int)layerFiles[carry].size())
                break;
            indices[carry] = 0;
            carry--;
        }
        if (carry < 0)
            break;
    }

    std::cout << "Generated " << iconIndex << " icons.\n";

    // ----- Write atlas to file -----
    std::filesystem::create_directories("GameData/Icons");
    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        std::perror("Cannot write output");
        return 4;
    }
    out.write(reinterpret_cast<const char *>(atlas.data()), atlas.size() * sizeof(u16));
    out.close();
    std::printf("Wrote %zu pixels to %s\n", atlas.size(), outputPath.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}