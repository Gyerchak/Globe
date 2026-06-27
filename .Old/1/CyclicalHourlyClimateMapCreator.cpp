/**
 * CyclicalHourlyClimateMapCreator – Binary 32‑bit per‑pixel climate data
 * -----------------------------------------------------------------------
 * Reads five 8192×8192 PNGs (red channel only) from the input folder
 * and packs each pixel's HourOfTheYear (0 initially, range 0‑8639),
 * HourlyTemperature, HourlyHumidity, HourlyWind, HourlyWindDir, and
 * HourlyPressure into a single 32‑bit integer. The result is written
 * as a raw binary file.
 *
 * HourOfTheYear is never read from a PNG – it is fixed to 0.
 * In the game engine it can range 0‑8639, wrapping to 0 after 8640.
 *
 * Auto‑mode expects the PNGs in:
 *   GameFiles/Map/CyclicalClimate/
 *
 * Manual mode:
 *   CyclicalHourlyClimateMapCreator.exe temperature.png humidity.png
 *     wind.png winddir.png pressure.png output.bin
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

    std::string names[5] = {
        "HourlyTemperatureMap.png",
        "HourlyHumidityMap.png",
        "HourlyWindMap.png",
        "HourlyWindDirMap.png",
        "HourlyPressureMap.png"};

    int max_vals[5] = {
        255, // Temperature (8 bit)
        3,   // Humidity    (2 bit)
        7,   // Wind        (3 bit)
        7,   // WindDir     (3 bit)
        3    // Pressure    (2 bit)
    };

    std::string folder;
    std::string output_path;

    if (argc == 1)
    {
        folder = "GameFiles/Map/CyclicalClimate";
        output_path = "GameData/Map/CyclicalHourlyClimate.bin";
    }
    else if (argc == 7)
    {
        folder = "";
        for (int i = 0; i < 5; ++i)
            names[i] = argv[1 + i];
        output_path = argv[6];
    }
    else
    {
        std::fprintf(stderr,
                     "Usage: %s [temperature.png humidity.png wind.png winddir.png pressure.png output.bin]\n",
                     argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    std::vector<u32> maps[5];
    int width = 0, height = 0;

    for (int i = 0; i < 5; ++i)
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
    std::vector<u32> out(pixelCount);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        auto scale = [&](u32 red, int max_val) -> u32
        {
            return (u32)((red * (unsigned)max_val + 127u) / 255u);
        };

        u32 val = 0;
        val |= scale(maps[0][i], max_vals[0]) << 10; // Temperature
        val |= scale(maps[1][i], max_vals[1]) << 8;  // Humidity
        val |= scale(maps[2][i], max_vals[2]) << 5;  // Wind
        val |= scale(maps[3][i], max_vals[3]) << 2;  // WindDir
        val |= scale(maps[4][i], max_vals[4]) << 0;  // Pressure

        out[i] = val;
    }

    write_binary(output_path, width, height, out);
    std::printf("Wrote %zu pixels to %s\n", pixelCount, output_path.c_str());

    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}