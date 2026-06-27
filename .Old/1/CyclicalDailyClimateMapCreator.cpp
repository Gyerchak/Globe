/**
 * CyclicalDailyClimateMapCreator – Binary 16‑bit per‑pixel daily climate data
 * ---------------------------------------------------------------------------
 * Reads two 8192×8192 PNGs (red channel only) from the input folder and packs
 * each pixel’s DayOfTheYear (0 initially, range 0‑359), DailyLocalSeason,
 * and DailyWeather into a single 16‑bit integer. The result is written as a
 * raw binary file.
 *
 * DayOfTheYear is never read from a PNG – it is fixed to 0.
 * In the game engine it ranges 0‑359, wrapping to 0 after 360.
 *
 * Auto‑mode expects the PNGs in:
 *   GameFiles/Map/CyclicalClimate/
 *
 * Manual mode:
 *   CyclicalDailyClimateMapCreator.exe dailyseason.png dailyweather.png output.bin
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

    std::string names[2] = {
        "DailyLocalSeasonMap.png",
        "DailyWeatherMap.png"};

    int max_vals[2] = {
        7, // DailyLocalSeason (3 bit)
        15 // DailyWeather     (4 bit)
    };

    std::string folder;
    std::string output_path;

    if (argc == 1)
    {
        folder = "GameFiles/Map/CyclicalClimate";
        output_path = "GameData/Map/CyclicalDailyClimate.bin";
    }
    else if (argc == 4)
    {
        folder = "";
        for (int i = 0; i < 2; ++i)
            names[i] = argv[1 + i];
        output_path = argv[3];
    }
    else
    {
        std::fprintf(stderr,
                     "Usage: %s [dailyseason.png dailyweather.png output.bin]\n",
                     argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    std::vector<u32> maps[2];
    int width = 0, height = 0;

    for (int i = 0; i < 2; ++i)
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
        val |= scale(maps[0][i], max_vals[0]) << 4; // DailyLocalSeason
        val |= scale(maps[1][i], max_vals[1]) << 0; // DailyWeather

        out[i] = val;
    }

    write_binary(output_path, width, height, out);
    std::printf("Wrote %zu pixels to %s\n", pixelCount, output_path.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}