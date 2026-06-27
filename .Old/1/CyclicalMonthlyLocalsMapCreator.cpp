/**
 * CyclicalMonthlyLocalsMapCreator – Binary 32‑bit per‑pixel native data
 * -----------------------------------------------------------------------
 * Reads two 8192×8192 PNGs: MonthlyPopulationIdMap.png (Red+Green channels
 * combined to 16‑bit PopulationID) and MonthlyLocalsNumMap.png (Red channel
 * scaled to 2‑bit LocalsNum). Packs them into a single 32‑bit integer per
 * pixel, written as a raw binary file.
 *
 * Auto‑mode (double‑click) expects the PNGs in:
 *   GameFiles/Map/CyclicalLocals/
 *
 * Manual mode:
 *   CyclicalMonthlyLocalsMapCreator.exe populationid.png Localsnum.png output.bin
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using u32 = uint32_t;
using u16 = uint16_t;

// Load Red+Green channels combined into a 16‑bit value (0–65535).
static bool load_rg_to_u16(const std::string &path,
                           std::vector<u16> &out,
                           int &width, int &height)
{
    int w = 0, h = 0, c = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false;
    out.resize((size_t)w * (size_t)h);
    for (size_t i = 0; i < (size_t)w * (size_t)h; ++i)
    {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        out[i] = (u16)(((u16)r << 8) | (u16)g);
    }
    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

// Load Red channel only.
static bool load_red_channel(const std::string &path,
                             std::vector<u32> &out,
                             int &width, int &height)
{
    int w = 0, h = 0, c = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false;
    out.resize((size_t)w * (size_t)h);
    for (size_t i = 0; i < (size_t)w * (size_t)h; ++i)
        out[i] = (u32)data[i * 4];
    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

static void write_binary(const std::filesystem::path &out,
                         int width, int height,
                         const std::vector<u32> &values)
{
    FILE *f = std::fopen(out.string().c_str(), "wb");
    if (!f)
    {
        std::perror("fopen");
        return;
    }
    std::fwrite(values.data(), sizeof(u32), values.size(), f);
    std::fclose(f);
}

int main(int argc, char **argv)
{
    const int EXPECTED_W = 8192;
    const int EXPECTED_H = 8192;

    std::string popPath;
    std::string nativePath;
    std::string folder;
    std::string output_path;

    if (argc == 1)
    {
        folder = "GameFiles/Map/CyclicalLocals";
        popPath = folder + "/MonthlyPopulationIdMap.png";
        nativePath = folder + "/MonthlyLocalsNumMap.png";
        output_path = "GameData/Map/CyclicalMonthlyLocalsMap.bin"; // <-- renamed
    }
    else if (argc == 4)
    {
        popPath = argv[1];
        nativePath = argv[2];
        output_path = argv[3];
    }
    else
    {
        std::fprintf(stderr,
                     "Usage: %s [populationid.png Localsnum.png output.bin]\n",
                     argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    // Load PopulationID (Red+Green → 16‑bit)
    std::vector<u16> popMap;
    int width = 0, height = 0;
    if (!load_rg_to_u16(popPath, popMap, width, height))
    {
        std::fprintf(stderr, "Failed to load %s\n", popPath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (width != EXPECTED_W || height != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     popPath.c_str(), width, height, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    // Load LocalsNum (Red → 0–255)
    std::vector<u32> nativeMap;
    int w2 = 0, h2 = 0;
    if (!load_red_channel(nativePath, nativeMap, w2, h2))
    {
        std::fprintf(stderr, "Failed to load %s\n", nativePath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (w2 != EXPECTED_W || h2 != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     nativePath.c_str(), w2, h2, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    size_t pixelCount = (size_t)width * height;
    std::vector<u32> out(pixelCount);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        u32 popID = popMap[i];                                 // 0–65535
        u32 native = (u32)((nativeMap[i] * 3u + 127u) / 255u); // 0–3

        u32 val = 0;
        // Layout:
        //   bits 17‑ 2 : PopulationID (16 bits)
        //   bits  1‑ 0 : LocalsNum   (2 bits)
        val |= (popID & 0xFFFFu) << 2;
        val |= (native & 0x3u);

        out[i] = val;
    }

    write_binary(output_path, width, height, out);
    std::printf("Wrote %zu pixels to %s\n", pixelCount, output_path.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}