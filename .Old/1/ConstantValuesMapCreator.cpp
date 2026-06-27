/**
 * ConstantValuesMapCreator – Binary 16‑bit per‑pixel value scores
 * ---------------------------------------------------------------
 * Reads eight 8192×8192 PNGs (red channel only) and packs each pixel’s
 * Comfort, Development, Prosperity, Safety, Health, Knowledge, Capacity,
 * and Score (each 2 bits) into a single 16‑bit integer. The result is
 * written as a raw binary file, one uint16_t per pixel, row‑major order.
 *
 * Auto‑mode (double‑click) expects the PNGs in:
 *   GameFiles/Map/ConstantValues/
 *
 * Manual mode:
 *   ConstantValuesMapCreator.exe comfort.png development.png prosperity.png
 *     safety.png health.png knowledge.png capacity.png score.png output.bin
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
                         const std::vector<u16> &values)
{
    FILE *f = std::fopen(out.string().c_str(), "wb");
    if (!f)
    {
        std::perror("fopen");
        return;
    }
    std::fwrite(values.data(), sizeof(u16), values.size(), f);
    std::fclose(f);
}

int main(int argc, char **argv)
{
    const int EXPECTED_W = 8192;
    const int EXPECTED_H = 8192;

    // File names in the order of the bit layout
    std::string names[8] = {
        "ComfortMap.png",
        "DevelopmentMap.png",
        "ProsperityMap.png",
        "SafetyMap.png",
        "HealthMap.png",
        "KnowledgeMap.png",
        "CapacityMap.png",
        "ScoreMap.png"};

    int max_vals[8] = {
        3, // Comfort      (2 bit)
        3, // Development  (2 bit)
        3, // Prosperity   (2 bit)
        3, // Safety       (2 bit)
        3, // Health       (2 bit)
        3, // Knowledge    (2 bit)
        3, // Capacity     (2 bit)
        3  // Score        (2 bit)
    };

    std::string folder;
    std::string output_path;

    if (argc == 1)
    {
        folder = "GameFiles/Map/ConstantValues";
        output_path = "GameData/Map/ConstantValuesMap.bin";
    }
    else if (argc == 10)
    { // 8 PNGs + output
        folder = "";
        for (int i = 0; i < 8; ++i)
            names[i] = argv[1 + i];
        output_path = argv[9];
    }
    else
    {
        std::fprintf(stderr,
                     "Usage: %s comfort.png development.png prosperity.png "
                     "safety.png health.png knowledge.png capacity.png score.png output.bin\n",
                     argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    std::vector<u32> maps[8];
    int width = 0, height = 0;

    for (int i = 0; i < 8; ++i)
    {
        std::string fullPath = folder.empty() ? names[i] : folder + "/" + names[i];
        int w = 0, h = 0;
        if (!load_red_channel(fullPath, maps[i], w, h))
        {
            std::fprintf(stderr, "Failed to load %s\n", fullPath.c_str());
            std::printf("\nPress Enter to exit...\n");
            std::getchar();
            return 2;
        }
        if (w != EXPECTED_W || h != EXPECTED_H)
        {
            std::fprintf(stderr,
                         "Size mismatch: %s is %dx%d, expected %dx%d\n",
                         fullPath.c_str(), w, h, EXPECTED_W, EXPECTED_H);
            std::printf("\nPress Enter to exit...\n");
            std::getchar();
            return 3;
        }
        if (i == 0)
        {
            width = w;
            height = h;
        }
    }

    size_t pixelCount = (size_t)width * height;
    std::vector<u16> out(pixelCount);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        auto scale = [&](u32 red, int max_val) -> u16
        {
            return (u16)((red * (unsigned)max_val + 127u) / 255u);
        };

        u16 val = 0;
        // Shift layout (MSB to LSB):
        //   bits 15‑14 : Comfort       (2 bits)  shift 14
        //   bits 13‑12 : Development   (2 bits)  shift 12
        //   bits 11‑10 : Prosperity    (2 bits)  shift 10
        //   bits  9‑ 8 : Safety        (2 bits)  shift  8
        //   bits  7‑ 6 : Health        (2 bits)  shift  6
        //   bits  5‑ 4 : Knowledge     (2 bits)  shift  4
        //   bits  3‑ 2 : Capacity      (2 bits)  shift  2
        //   bits  1‑ 0 : Score         (2 bits)  shift  0
        val |= scale(maps[0][i], max_vals[0]) << 14;
        val |= scale(maps[1][i], max_vals[1]) << 12;
        val |= scale(maps[2][i], max_vals[2]) << 10;
        val |= scale(maps[3][i], max_vals[3]) << 8;
        val |= scale(maps[4][i], max_vals[4]) << 6;
        val |= scale(maps[5][i], max_vals[5]) << 4;
        val |= scale(maps[6][i], max_vals[6]) << 2;
        val |= scale(maps[7][i], max_vals[7]) << 0;

        out[i] = val;
    }

    write_binary(output_path, width, height, out);
    std::printf("Wrote %zu pixels to %s\n", pixelCount, output_path.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}