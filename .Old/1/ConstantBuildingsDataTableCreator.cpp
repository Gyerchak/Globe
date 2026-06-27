/**
 * ConstantBuildingsDataTableCreator – Buildings × Province lookup (16‑bit province)
 * ---------------------------------------------------------------------------------
 * 65536 provinces × 256 buildings × 1 byte = 16 MiB.
 * All entries initialised to 0.
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>

int main(int argc, char **argv)
{
    std::string output_path;
    if (argc == 1)
    {
        output_path = "GameData/RgTable/BuildingsData.bin";
    }
    else
    {
        std::fprintf(stderr, "Usage: %s  (auto‑mode only)\n", argv[0]);
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    const uint32_t NUM_PROVINCES = 1U << 16; // 65536
    const uint32_t NUM_BUILDINGS = 256;
    const uint64_t TOTAL_BYTES = (uint64_t)NUM_PROVINCES * NUM_BUILDINGS; // 16 MiB

    std::ofstream out(output_path, std::ios::binary);
    if (!out)
    {
        std::perror("Cannot open output file");
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    // Write all provinces silently – the file is small
    const uint8_t zeroBlock[256] = {0};
    for (uint32_t province = 0; province < NUM_PROVINCES; ++province)
        out.write(reinterpret_cast<const char *>(zeroBlock), sizeof(zeroBlock));

    out.close();
    std::printf("Wrote %llu bytes to %s\n",
                (unsigned long long)TOTAL_BYTES, output_path.c_str());
    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}