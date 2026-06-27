/**
 * ConstantProvincesMapCreator – 64‑bit per‑pixel province + country + location
 * ----------------------------------------------------------------------------
 * Reads three 8192×8192 PNGs:
 *   ProvinceMap.png (Red+Green → 16‑bit Province)
 *   CountryMap.png  (Red → 8‑bit Country)
 *   LocationMap.png (Red → LocationType 4‑bit, Green+Blue → LocationLevel 10‑bit)
 *
 * LocalisationX and LocalisationY are initialised to 0.
 *
 * Output: GameData/Map/ConstantProvincesMap.bin (512 MiB)
 *
 * Auto‑mode expects the PNGs in:
 *   GameFiles/Map/ConstantProvinces/
 */

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

// Load Province as 16‑bit from Red+Green channels
static bool load_rg_to_u16(const std::string &path, std::vector<u16> &out,
                           int &width, int &height)
{
    int w = 0, h = 0, c = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false;
    out.resize((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; ++i)
    {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        out[i] = ((u16)r << 8) | (u16)g;
    }
    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

// Load a single channel (Red) as 8‑bit values
static bool load_red_channel(const std::string &path, std::vector<u8> &out,
                             int &width, int &height)
{
    int w = 0, h = 0, c = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false;
    out.resize((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; ++i)
        out[i] = (u8)data[i * 4 + 0];
    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

// Load Red, Green, and Blue channels from a PNG
static bool load_rgb_channels(const std::string &path,
                              std::vector<u32> &red,
                              std::vector<u32> &green,
                              std::vector<u32> &blue,
                              int &width, int &height)
{
    int w = 0, h = 0, c = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false;
    size_t count = (size_t)w * h;
    red.resize(count);
    green.resize(count);
    blue.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        red[i] = (u32)data[i * 4 + 0];
        green[i] = (u32)data[i * 4 + 1];
        blue[i] = (u32)data[i * 4 + 2];
    }
    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

static void write_binary(const std::filesystem::path &out_path,
                         int width, int height, const std::vector<u64> &values)
{
    FILE *f = std::fopen(out_path.string().c_str(), "wb");
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

    std::string provincePath, countryPath, locationPath;
    std::string output_path;
    std::string folder;

    if (argc == 1)
    {
        folder = "GameFiles/Map/ConstantProvinces";
        provincePath = folder + "/ProvinceMap.png";
        countryPath = folder + "/CountryMap.png";
        locationPath = folder + "/LocationMap.png";
        output_path = "GameData/Map/ConstantProvincesMap.bin";
    }
    else if (argc == 5)
    {
        provincePath = argv[1];
        countryPath = argv[2];
        locationPath = argv[3];
        output_path = argv[4];
    }
    else
    {
        std::fprintf(stderr, "Usage: %s [province.png country.png location.png output.bin]\n", argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    // ---- Load Province (16‑bit RG) ----
    std::vector<u16> province;
    int width = 0, height = 0;
    if (!load_rg_to_u16(provincePath, province, width, height))
    {
        std::fprintf(stderr, "Failed to load %s\n", provincePath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (width != EXPECTED_W || height != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     provincePath.c_str(), width, height, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    // ---- Load Country (Red channel) ----
    std::vector<u8> country;
    int w2 = 0, h2 = 0;
    if (!load_red_channel(countryPath, country, w2, h2))
    {
        std::fprintf(stderr, "Failed to load %s\n", countryPath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (w2 != EXPECTED_W || h2 != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     countryPath.c_str(), w2, h2, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    // ---- Load LocationMap (Red, Green, Blue) ----
    std::vector<u32> locRed, locGreen, locBlue;
    int w3 = 0, h3 = 0;
    if (!load_rgb_channels(locationPath, locRed, locGreen, locBlue, w3, h3))
    {
        std::fprintf(stderr, "Failed to load %s\n", locationPath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (w3 != EXPECTED_W || h3 != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     locationPath.c_str(), w3, h3, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    // ---- Pack everything ----
    size_t pixelCount = (size_t)width * height;
    std::vector<u64> packed(pixelCount);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        u64 prov = province[i]; // 16 bits
        u64 cnt = country[i];   // 8 bits

        // LocationType from Red (0‑255) scaled to 0‑15
        u64 locType = (locRed[i] * 15u + 127u) / 255u;

        // LocationLevel from Green+Blue combined (16‑bit), then scaled down to 10 bits
        u32 greenBlue = ((u32)locGreen[i] << 8) | (u32)locBlue[i]; // 0‑65535
        u64 locLev = greenBlue >> 6;                               // effectively (greenBlue * 1023) / 65535, fast and lossless

        u64 val = 0;
        val |= (prov & 0xFFFFu) << 48; // bits 63‑48
        val |= (cnt & 0xFFu) << 40;    // bits 47‑40
        // X (bits 39‑27) = 0
        // Y (bits 26‑14) = 0
        val |= (locType & 0xFu) << 10; // bits 13‑10
        val |= (locLev & 0x3FFu) << 0; // bits 9‑0

        packed[i] = val;
    }

    write_binary(output_path, width, height, packed);
    std::printf("Wrote %zu pixels to %s\n", pixelCount, output_path.c_str());
    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}