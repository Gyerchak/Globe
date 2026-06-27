/**
 * Map Creator – PNG to 64‑bit CSV Converter
 * ==========================================
 * Reads eleven per‑pixel map PNGs (each representing a single channel of data)
 * and packs them into a single 64‑bit value per pixel. The result is written
 * as a CSV file that can be consumed by the companion map viewer.
 *
 * Bit layout (MSB → LSB, 64 bits total):
 *   [SeaLand:1][Height:8][Country:8][Province:8][Temperature:6]
 *   [AreaType:6][Forest:6][Soil:6][ResID:6][ResNum:6][Infra:3]
 *
 * Channel extraction from the input PNGs:
 *   - SeaLand       → Black‑White (any non‑zero value = land)
 *   - Height        → Grayscale (full 0‑255)
 *   - Country       → RED channel (full 0‑255)
 *   - Province      → GREEN channel (full 0‑255)
 *   - Temperature   → RED channel, use values 192‑255 (mapped to 0‑63)
 *   - AreaType      → GREEN channel, use values 192‑255 (mapped to 0‑63)
 *   - Forest        → GREEN channel, use values 192‑255 (mapped to 0‑63)
 *   - Soil          → GREEN channel, use values 192‑255 (mapped to 0‑63)
 *   - ResID         → GREEN channel, use values 192‑255 (mapped to 0‑63)
 *   - ResNum        → Grayscale, use values 192‑255 (mapped to 0‑63)
 *   - Infra         → RED channel, use values 248‑255 (mapped to 0‑7)
 *
 * The program has two modes:
 *   1. AUTO MODE (double‑click, no arguments) – looks for the eleven PNGs in a
 *      hard‑coded relative folder and writes the CSV there as well.
 *   2. MANUAL MODE (command‑line) – accepts twelve arguments (eleven PNG paths +
 *      output path) and an optional --mode flag.
 */

#include <cstdint>    // Fixed‑width integer types (uint8_t, uint64_t, …)
#include <cstdio>     // C standard I/O (fopen, fprintf, fclose, perror)
#include <cstdlib>    // General utilities
#include <cstring>    // C string functions (std::strcmp)
#include <string>     // C++ string (std::string)
#include <vector>     // Dynamic array (std::vector)
#include <filesystem> // Filesystem operations (paths, create_directories)
#include <iostream>   // Console I/O (std::cerr, std::printf)

// Make stb_image implement its functions in this translation unit.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Public‑domain single‑header image loader

// ---------------------------------------------------------------------------
// Convenience type aliases (shorter names for standard unsigned integers)
// ---------------------------------------------------------------------------
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

// ============================================================================
// 64‑bit Packing
// ============================================================================

/**
 * pack_fields
 * -----------
 * Combines eleven separate map values into a single 64‑bit integer according
 * to a fixed bit layout. Each value is masked to its maximum bit width,
 * shifted into position, and ORed together.
 *
 * Layout (from MSB to LSB):
 *   bits 63     : SeaLand     (1 bit)   shift 63
 *   bits 62‑55  : Height      (8 bits)  shift 55
 *   bits 54‑47  : Country     (8 bits)  shift 47
 *   bits 46‑39  : Province    (8 bits)  shift 39
 *   bits 38‑33  : Temperature (6 bits)  shift 33
 *   bits 32‑27  : AreaType    (6 bits)  shift 27
 *   bits 26‑21  : Forest      (6 bits)  shift 21
 *   bits 20‑15  : Soil        (6 bits)  shift 15
 *   bits 14‑ 9  : ResID       (6 bits)  shift  9
 *   bits  8‑ 3  : ResNum      (6 bits)  shift  3
 *   bits  2‑ 0  : Infra       (3 bits)  shift  0
 *
 * @param seal          Sea/Land flag (0 = sea, 1 = land)
 * @param height        Height value (0‑255, grayscale)
 * @param country       Country value (0‑255)
 * @param province      Province value (0‑255)
 * @param temperature   Temperature value (0‑63)
 * @param areatype      AreaType value (0‑63)
 * @param forest        Forest value (0‑63)
 * @param soil          Soil value (0‑63)
 * @param resid         ResID value (0‑63)
 * @param resnum        ResNum value (0‑63, grayscale)
 * @param infra         Infrastructure value (0‑7)
 * @return              Packed 64‑bit value
 */
static inline u64 pack_fields(u64 seal, u64 height, u64 country, u64 province,
                              u64 temperature, u64 areatype, u64 forest,
                              u64 soil, u64 resid, u64 resnum, u64 infra)
{
    u64 v = 0;
    v |= (seal & 0x1ULL) << 63;
    v |= (height & 0xFFULL) << 55;
    v |= (country & 0xFFULL) << 47;
    v |= (province & 0xFFULL) << 39;
    v |= (temperature & 0x3FULL) << 33;
    v |= (areatype & 0x3FULL) << 27;
    v |= (forest & 0x3FULL) << 21;
    v |= (soil & 0x3FULL) << 15;
    v |= (resid & 0x3FULL) << 9;
    v |= (resnum & 0x3FULL) << 3;
    v |= (infra & 0x7ULL) << 0;
    return v;
}

// ============================================================================
// PNG Loading & Channel Extraction
// ============================================================================

/**
 * load_channel_to_u32
 * -------------------
 * Loads a PNG image from disk, forces it to RGBA (4 channels), extracts a
 * single colour channel (0=R, 1=G, 2=B, 3=A) into a flat vector of u32
 * values, and returns the image dimensions.
 *
 * @param path     File path to the PNG.
 * @param out      Output vector that receives the extracted channel values.
 * @param width    (out) Image width in pixels.
 * @param height   (out) Image height in pixels.
 * @param channel  Which colour channel to extract (0=Red, 1=Green, 2=Blue).
 * @return         true on success, false if the file could not be loaded.
 */
static bool load_channel_to_u32(const std::string &path,
                                std::vector<u32> &out,
                                int &width, int &height,
                                int channel)
{
    int w = 0, h = 0, c = 0;
    // Force 4 channels (RGBA) so the pixel stride is always 4 bytes.
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data)
        return false; // load failed (file not found, etc.)

    out.resize((size_t)w * (size_t)h);
    for (size_t i = 0; i < (size_t)w * (size_t)h; ++i)
        out[i] = (u32)data[i * 4 + channel]; // extract the requested channel

    stbi_image_free(data);
    width = w;
    height = h;
    return true;
}

// ============================================================================
// CSV Output
// ============================================================================

/**
 * write_csv
 * ---------
 * Writes a flat CSV file with a header row "x,y,value_hex64" and one row per
 * pixel. The 64‑bit value is formatted as a 16‑digit uppercase hex number
 * with a "0x" prefix.
 *
 * @param out     Output file path.
 * @param width   Number of columns.
 * @param height  Number of rows.
 * @param values  Flat array of packed 64‑bit values (row‑major order).
 */
static void write_csv(const std::filesystem::path &out,
                      int width, int height,
                      const std::vector<u64> &values)
{
    FILE *f = std::fopen(out.string().c_str(), "w");
    if (!f)
    {
        std::perror("fopen"); // print OS error (e.g., "Permission denied")
        return;
    }

    // Header line
    std::fprintf(f, "x,y,value_hex64\n");

    // Walk every pixel in row‑major order and write x,y,0xHH…
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            u64 v = values[y * (size_t)width + x];
            // %016llX → zero‑padded, 16‑digit, uppercase hex.
            std::fprintf(f, "%d,%d,0x%016llX\n", x, y, (unsigned long long)v);
        }

    std::fclose(f);
}

/**
 * write_per_location
 * ------------------
 * Alternative output mode: creates a directory with one small text file per
 * pixel. The file name is "X;Y;data.txt" and contains a single line with the
 * 64‑bit value in hex. This can be useful for importing into other tools that
 * expect per‑cell files.
 *
 * @param out_dir  Output directory path (created if needed).
 * @param width    Number of columns.
 * @param height   Number of rows.
 * @param values   Flat array of packed 64‑bit values.
 */
static void write_per_location(const std::filesystem::path &out_dir,
                               int width, int height,
                               const std::vector<u64> &values)
{
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec); // mkdir -p equivalent
    if (ec)
    {
        std::cerr << "Could not create output dir: " << ec.message() << "\n";
        return;
    }

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            u64 v = values[y * (size_t)width + x];

            // Build a file name like "12;34;data.txt"
            char idbuf[64];
            std::snprintf(idbuf, sizeof(idbuf), "%d;%d", x, y);
            std::string color_name = (out_dir / (std::string(idbuf) + ";data.txt")).string();

            FILE *df = std::fopen(color_name.c_str(), "w");
            if (df)
            {
                std::fprintf(df, "0x%016llX\n", (unsigned long long)v);
                std::fclose(df);
            }
        }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv)
{
    // ------------------------------------------------------------------------
    // AUTO MODE (no command‑line arguments)
    // ------------------------------------------------------------------------
    if (argc == 1)
    {
        // Hard‑coded folder containing the eleven PNGs (relative to the exe).
        std::string folder = "GameFiles/Map/BaseMap";
        // Where the output CSV will be written.
        std::string output = "GameFiles/Map/BaseMap.csv";
        std::string mode = "csv"; // can be "csv" or "per-location"

        // Exact filenames as they appear in the folder (order must match the bit layout).
        std::string names[11] = {
            "SeaLandMap.png",       // 1 bit  – Black‑White (any non‑zero = land)
            "HeightMap.png",        // 8 bits – Grayscale (full 0‑255)
            "CountryMap.png",       // 8 bits – RED channel (0‑255)
            "ProvinceMap.png",      // 8 bits – GREEN channel (0‑255)
            "TemperatureMap.png",   // 6 bits – RED channel, paint 192‑255
            "AreaTypeMap.png",      // 6 bits – GREEN channel, paint 192‑255
            "ForestMap.png",        // 6 bits – GREEN channel, paint 192‑255
            "SoilMap.png",          // 6 bits – GREEN channel, paint 192‑255
            "ResIDMap.png",         // 6 bits – GREEN channel, paint 192‑255
            "ResNumMap.png",        // 6 bits – Grayscale, paint 192‑255
            "InfrastructureMap.png" // 3 bits – RED channel, paint 248‑255
        };

        // Which colour channel to extract from each PNG.
        // 0 = Red, 1 = Green, 2 = Blue.
        const int channels[11] = {0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0}; // <-- Infra now RED (0)

        int width = 0, height = 0;
        std::vector<u32> maps[11]; // one vector per input map

        // Load all eleven PNGs.
        for (int i = 0; i < 11; ++i)
        {
            std::string fullPath = folder + "/" + names[i];
            int w = 0, h = 0;
            if (!load_channel_to_u32(fullPath, maps[i], w, h, channels[i]))
            {
                std::fprintf(stderr, "Failed to load %s\n", fullPath.c_str());
                std::printf("\nPress Enter to exit...\n");
                std::getchar();
                return 2;
            }

            // The first image determines the grid dimensions.
            if (i == 0)
            {
                width = w;
                height = h;
            }
            // All subsequent images must have the same size.
            else if (w != width || h != height)
            {
                std::fprintf(stderr,
                             "Size mismatch: %s (%dx%d) vs first (%dx%d)\n",
                             fullPath.c_str(), w, h, width, height);
                std::printf("\nPress Enter to exit...\n");
                std::getchar();
                return 3;
            }
        }

        // Pack each pixel.
        size_t pixelCount = (size_t)width * height;
        std::vector<u64> out(pixelCount);

        for (size_t i = 0; i < pixelCount; ++i)
        {
            // SeaLand: any non‑zero value → land (1), else sea (0)
            u64 seal = (maps[0][i] > 0) ? 1ULL : 0ULL;

            // 8‑bit layers – full 0‑255
            u64 height_v = maps[1][i] & 0xFFULL;   // Grayscale
            u64 country_v = maps[2][i] & 0xFFULL;  // RED
            u64 province_v = maps[3][i] & 0xFFULL; // GREEN

            // 6‑bit layers: extract from the upper 64 colours (192‑255)
            // 192 → 0, 193 → 1, … 255 → 63. Values below 192 become 0.
            u64 temperature = (maps[4][i] >= 192) ? (maps[4][i] - 192) : 0;
            u64 areatype_v = (maps[5][i] >= 192) ? (maps[5][i] - 192) : 0;
            u64 forest_v = (maps[6][i] >= 192) ? (maps[6][i] - 192) : 0;
            u64 soil_v = (maps[7][i] >= 192) ? (maps[7][i] - 192) : 0;
            u64 resid_v = (maps[8][i] >= 192) ? (maps[8][i] - 192) : 0;
            u64 resnum_v = (maps[9][i] >= 192) ? (maps[9][i] - 192) : 0; // Grayscale

            // 3‑bit layer: extract from the upper 8 colours (248‑255)
            // 248 → 0, 249 → 1, … 255 → 7. Values below 248 become 0.
            u64 infra_v = (maps[10][i] >= 248) ? (maps[10][i] - 248) : 0;

            out[i] = pack_fields(seal, height_v, country_v, province_v,
                                 temperature, areatype_v, forest_v,
                                 soil_v, resid_v, resnum_v, infra_v);
        }

        // Write the output in the selected format.
        if (mode == "csv")
        {
            write_csv(output, width, height, out);
            std::printf("Wrote CSV with %zu cells to %s\n",
                        pixelCount, output.c_str());
        }
        else
        {
            write_per_location(output, width, height, out);
            std::printf("Wrote per-location to %s (%zu cells)\n",
                        output.c_str(), pixelCount);
        }

        // Pause so the user can read the result (window stays open).
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 0;
    }

    // ------------------------------------------------------------------------
    // MANUAL MODE (command‑line arguments)
    // ------------------------------------------------------------------------
    // Expect at least 12 arguments: 11 PNGs + output path.
    if (argc < 12)
    {
        std::fprintf(stderr,
                     "Usage: %s seal.png height.png country.png province.png "
                     "temperature.png areatype.png forest.png soil.png "
                     "resid.png resnum.png infra.png output [--mode csv|per-location]\n",
                     argv[0]);
        std::fprintf(stderr,
                     "Paint ranges: seal=Black‑White, height=0‑255, country=0‑255, "
                     "province=0‑255, temperature=192‑255, areatype=192‑255, "
                     "forest=192‑255, soil=192‑255, resid=192‑255, resnum=192‑255, infra=248‑255\n");
        std::printf("\nPress Enter to exit...\n");
        std::getchar();
        return 1;
    }

    // Collect the eleven input paths from argv[1]..argv[11].
    std::string paths[11];
    for (int i = 0; i < 11; ++i)
        paths[i] = argv[1 + i];

    // argv[12] is the output path.
    std::string output = argv[12];
    std::string mode = "csv";

    // Optional --mode flag (argv[13] onwards).
    for (int i = 13; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            mode = argv[++i];
    }

    int width = 0, height = 0;
    std::vector<u32> maps[11];
    const int channels[11] = {0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0}; // <-- Infra now RED (0)

    // Same loading and validation as auto mode.
    for (int i = 0; i < 11; ++i)
    {
        int w = 0, h = 0;
        if (!load_channel_to_u32(paths[i], maps[i], w, h, channels[i]))
        {
            std::fprintf(stderr, "Failed to load %s\n", paths[i].c_str());
            std::printf("\nPress Enter to exit...\n");
            std::getchar();
            return 2;
        }
        if (i == 0)
        {
            width = w;
            height = h;
        }
        else if (w != width || h != height)
        {
            std::fprintf(stderr,
                         "Size mismatch: %s (%dx%d) vs first (%dx%d)\n",
                         paths[i].c_str(), w, h, width, height);
            std::printf("\nPress Enter to exit...\n");
            std::getchar();
            return 3;
        }
    }

    size_t pixelCount = (size_t)width * height;
    std::vector<u64> out(pixelCount);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        u64 seal = (maps[0][i] > 0) ? 1ULL : 0ULL;
        u64 height_v = maps[1][i] & 0xFFULL;
        u64 country_v = maps[2][i] & 0xFFULL;
        u64 province_v = maps[3][i] & 0xFFULL;

        // 6‑bit layers: upper 64 colours (192‑255) mapped to 0‑63
        u64 temperature = (maps[4][i] >= 192) ? (maps[4][i] - 192) : 0;
        u64 areatype_v = (maps[5][i] >= 192) ? (maps[5][i] - 192) : 0;
        u64 forest_v = (maps[6][i] >= 192) ? (maps[6][i] - 192) : 0;
        u64 soil_v = (maps[7][i] >= 192) ? (maps[7][i] - 192) : 0;
        u64 resid_v = (maps[8][i] >= 192) ? (maps[8][i] - 192) : 0;
        u64 resnum_v = (maps[9][i] >= 192) ? (maps[9][i] - 192) : 0;

        // 3‑bit layer: upper 8 colours (248‑255) mapped to 0‑7
        u64 infra_v = (maps[10][i] >= 248) ? (maps[10][i] - 248) : 0;

        out[i] = pack_fields(seal, height_v, country_v, province_v,
                             temperature, areatype_v, forest_v,
                             soil_v, resid_v, resnum_v, infra_v);
    }

    if (mode == "csv")
    {
        write_csv(output, width, height, out);
        std::printf("Wrote CSV with %zu cells to %s\n",
                    pixelCount, output.c_str());
    }
    else
    {
        write_per_location(output, width, height, out);
        std::printf("Wrote per-location to %s (%zu cells)\n",
                    output.c_str(), pixelCount);
    }

    // Pause so the user can read the result in manual mode as well.
    std::printf("\nPress Enter to exit...\n");
    std::getchar();
    return 0;
}

/*
===========================================================================
BUILD INSTRUCTIONS (MinGW-w64 / Git Bash)
===========================================================================
1. Make sure "stb_image.h" is in the same folder as this file.
2. Open Git Bash in the project directory and run:

   g++ -std=c++20 -O2 -static -o map_creator.exe map_creator.cpp

   Explanation of flags:
     -std=c++20  : Use the C++20 standard (required for <filesystem>, etc.)
     -O2         : Optimise for speed and size.
     -static     : Link everything into a single .exe – no external DLLs needed.
     -o ...      : Name of the output executable.

===========================================================================
USAGE
===========================================================================
AUTO MODE (double‑click or run without arguments):
   The program expects eleven PNG files in the folder "GameFiles/Map/BaseMap/"
   (relative to the executable) with these exact names:
     SeaLandMap.png          HeightMap.png           CountryMap.png
     ProvinceMap.png         TemperatureMap.png      AreaTypeMap.png
     ForestMap.png           SoilMap.png             ResIDMap.png
     ResNumMap.png           InfrastructureMap.png
   It will create "GameFiles/Map/BaseMap.csv".
   A console window shows the progress and waits for Enter.

   PAINT RANGES:
     6‑bit layers: use colour values 192‑255 (the last 64 colours).
     3‑bit layers: use colour values 248‑255 (the last 8 colours).
     8‑bit layers: full 0‑255.
     SeaLand: any non‑black = land.

MANUAL MODE (command line):
   map_creator.exe seal.png height.png country.png province.png
                  temperature.png areatype.png forest.png soil.png
                  resid.png resnum.png infra.png output.csv
                  [--mode csv|per-location]
   Channel extraction:
     SealLand=Black‑White  Height=Grayscale   Country=RED
     Province=GREEN        Temperature=RED    AreaType=GREEN
     Forest=GREEN          Soil=GREEN         ResID=GREEN
     ResNum=Grayscale      Infra=RED

To change the default folder or filenames, edit the "folder" and "names"
variables near the top of main() in this source file.
===========================================================================
*/