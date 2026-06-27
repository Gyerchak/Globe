#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using u32 = uint32_t;
using u64 = uint64_t;

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
                         const std::vector<u64> &values)
{
    FILE *f = std::fopen(out.string().c_str(), "wb");
    if (!f)
    {
        std::perror("fopen");
        return;
    }
    std::fwrite(values.data(), sizeof(u64), values.size(), f);
    std::fclose(f);
}

int main(int argc, char **argv)
{
    const int EXPECTED_W = 8192;
    const int EXPECTED_H = 8192;

    std::string names[10] = {
        "WaterLandMap.png",
        "HeightMap.png",
        "TopographicMap.png",
        "ContinentalityMap.png",
        "CurrentsMap.png",
        "ClimateZoneMap.png",
        "ResIdMap.png",
        "ResNumMap.png",
        "ForestMap.png",
        "SoilMap.png"};

    int max_vals[10] = {
        3,   // WaterLand now 2 bits (0-3)
        255, // Height
        7,   // Topographic
        255, // Continentality
        7,   // Currents
        7,   // ClimateZone
        31,  // ResID
        3,   // ResNum
        3,   // Forest
        3    // Soil
    };

    std::string folder;
    std::string output_path;

    if (argc == 1)
    {
        folder = "GameFiles/Map/Static";
        output_path = "GameData/Map/BaseStaticMap.bin";
    }
    else if (argc == 12)
    {
        folder = "";
        for (int i = 0; i < 10; ++i)
            names[i] = argv[1 + i];
        output_path = argv[11];
    }
    else
    {
        std::fprintf(stderr,
                     "Usage: %s [waterland.png height.png topographic.png continentality.png "
                     "currents.png climatezone.png resid.png resnum.png forest.png soil.png output.bin]\n",
                     argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    std::vector<u32> maps[10];
    int width = 0, height = 0;

    for (int i = 0; i < 10; ++i)
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
    std::vector<u64> out(pixelCount);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        int x = (int)(i % width);
        int y = (int)(i / width);

        auto scale = [&](u32 red, int max_val) -> u64
        {
            if (max_val == 255)
                return (u64)(red & 0xFF);
            return (u64)((red * (unsigned)max_val + 127u) / 255u);
        };

        u64 val = 0;
        val |= (u64(x) & 0x1FFFu) << 51;
        val |= (u64(y) & 0x1FFFu) << 38;
        val |= scale(maps[0][i], max_vals[0]) << 36; // WaterLand (2 bits)
        val |= scale(maps[1][i], max_vals[1]) << 28; // Height
        val |= scale(maps[2][i], max_vals[2]) << 25; // Topographic
        val |= scale(maps[3][i], max_vals[3]) << 17; // Continentality
        val |= scale(maps[4][i], max_vals[4]) << 14; // Currents
        val |= scale(maps[5][i], max_vals[5]) << 11; // ClimateZone
        val |= scale(maps[6][i], max_vals[6]) << 6;  // ResID
        val |= scale(maps[7][i], max_vals[7]) << 4;  // ResNum
        val |= scale(maps[8][i], max_vals[8]) << 2;  // Forest
        val |= scale(maps[9][i], max_vals[9]) << 0;  // Soil

        out[i] = val;
    }

    write_binary(output_path, width, height, out);
    std::printf("Wrote %zu pixels to %s\n", pixelCount, output_path.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}