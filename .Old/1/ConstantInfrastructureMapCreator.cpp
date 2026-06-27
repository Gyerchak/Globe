/**
 * ConstantInfrastructureMapCreator – 4‑bit per‑pixel infrastructure data
 * ----------------------------------------------------------------------
 * Reads InfrastructureMap.png (Red channel, scaled to 0‑15) and writes
 * a packed binary where each byte contains two consecutive 4‑bit values
 * (first pixel in low nibble, second pixel in high nibble).
 *
 * Auto‑mode expects the PNG in:
 *   GameFiles/Map/ConstantInfrastructure/
 *
 * Manual mode:
 *   ConstantInfrastructureMapCreator.exe infra.png output.bin
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

    std::string inputPath;
    std::string output_path;
    std::string folder;

    if (argc == 1)
    {
        folder = "GameFiles/Map/ConstantInfrastructure";
        inputPath = folder + "/InfrastructureMap.png";
        output_path = "GameData/Map/ConstantInfrastructureMap.bin";
    }
    else if (argc == 3)
    {
        inputPath = argv[1];
        output_path = argv[2];
    }
    else
    {
        std::fprintf(stderr, "Usage: %s [infra.png output.bin]\n", argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    std::vector<u32> raw;
    int width = 0, height = 0;
    if (!load_red_channel(inputPath, raw, width, height))
    {
        std::fprintf(stderr, "Failed to load %s\n", inputPath.c_str());
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 2;
    }
    if (width != EXPECTED_W || height != EXPECTED_H)
    {
        std::fprintf(stderr, "Size mismatch: %s is %dx%d, expected %dx%d\n",
                     inputPath.c_str(), width, height, EXPECTED_W, EXPECTED_H);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 3;
    }

    size_t pixelCount = (size_t)width * height; // even number guaranteed
    std::vector<u8> packed(pixelCount / 2);     // 2 pixels per byte

    for (size_t i = 0; i < pixelCount; i += 2)
    {
        u8 infra1 = (u8)((raw[i] * 15u + 127u) / 255u);     // 0‑15
        u8 infra2 = (u8)((raw[i + 1] * 15u + 127u) / 255u); // 0‑15
        packed[i / 2] = (infra2 << 4) | infra1;             // first pixel in low nibble
    }

    write_binary(output_path, width, height, packed);
    std::printf("Wrote %zu bytes to %s\n", packed.size(), output_path.c_str());
    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}