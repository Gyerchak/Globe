/**
 * EventualVolcanicRiverMapCreator – 4‑bit per‑pixel volcanic + river data
 * ------------------------------------------------------------------------
 * Reads RiverMap.png and VolcanicMap.png (Red channel, scaled to 0‑3 each)
 * and packs two 4‑bit values per byte (first pixel's River+Volcanic in low
 * nibble, second pixel's in high nibble). Written as a raw binary file.
 *
 * Auto‑mode expects the PNGs in:
 *   GameFiles/Map/Eventual/
 *
 * Manual mode:
 *   EventualVolcanicRiverMapCreator.exe river.png volcanic.png output.bin
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using u32 = uint32_t;
using u8 = uint8_t;

static bool load_red_channel(const std::string &path, std::vector<u32> &out,
                             int &width, int &height)
{
    int w = 0, h = 0, c = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false;
    out.resize((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; ++i)
        out[i] = (u32)data[i * 4 + 0];
    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

static void write_binary(const std::filesystem::path &out_path,
                         int width, int height, const std::vector<u8> &packed)
{
    FILE *f = std::fopen(out_path.string().c_str(), "wb");
    if (!f)
    {
        std::perror("fopen");
        return;
    }
    std::fwrite(packed.data(), sizeof(u8), packed.size(), f);
    std::fclose(f);
}

int main(int argc, char **argv)
{
    const int EXPECTED_W = 8192;
    const int EXPECTED_H = 8192;

    std::string riverPath, volcanicPath;
    std::string output_path;
    std::string folder;

    if (argc == 1)
    {
        folder = "GameFiles/Map/Eventual";
        riverPath = folder + "/RiverMap.png";
        volcanicPath = folder + "/VolcanicMap.png";
        output_path = "GameData/Map/EventualVolcanicRiverMap.bin";
    }
    else if (argc == 4)
    {
        riverPath = argv[1];
        volcanicPath = argv[2];
        output_path = argv[3];
    }
    else
    {
        std::fprintf(stderr, "Usage: %s [river.png volcanic.png output.bin]\n", argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    // Load River
    std::vector<u32> river;
    int width = 0, height = 0;
    if (!load_red_channel(riverPath, river, width, height))
    {
        std::fprintf(stderr, "Failed to load %s\n", riverPath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (width != EXPECTED_W || height != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     riverPath.c_str(), width, height, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    // Load Volcanic
    std::vector<u32> volcanic;
    int w2 = 0, h2 = 0;
    if (!load_red_channel(volcanicPath, volcanic, w2, h2))
    {
        std::fprintf(stderr, "Failed to load %s\n", volcanicPath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (w2 != EXPECTED_W || h2 != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     volcanicPath.c_str(), w2, h2, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    size_t pixelCount = (size_t)width * height;
    std::vector<u8> packed(pixelCount / 2); // 2 pixels per byte

    for (size_t i = 0; i < pixelCount; i += 2)
    {
        // Pixel 1
        u8 r1 = (u8)((river[i] * 3u + 127u) / 255u);
        u8 v1 = (u8)((volcanic[i] * 3u + 127u) / 255u);
        u8 nib1 = (v1 << 2) | r1; // Volcanic in bits 3‑2, River in bits 1‑0

        // Pixel 2
        u8 r2 = (u8)((river[i + 1] * 3u + 127u) / 255u);
        u8 v2 = (u8)((volcanic[i + 1] * 3u + 127u) / 255u);
        u8 nib2 = (v2 << 2) | r2;

        packed[i / 2] = (nib2 << 4) | nib1; // first pixel in low nibble
    }

    write_binary(output_path, width, height, packed);
    std::printf("Wrote %zu bytes to %s\n", packed.size(), output_path.c_str());
    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}