// CompressHeightMap.cpp
// Location: Code/CompressHeightMap.cpp
// Compile: g++ -std=c++20 -static Code/CompressHeightMap.cpp -o Executables/CompressHeightMap.exe
// Double‑click the exe, or run: ./Executables/CompressHeightMap.exe

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>
#include <iterator>
#include <filesystem>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768; // 2:1 ratio – matches your source file

/* ============================================================================
   PROGRESSIVE BAND MAPPING – STEP‑BY‑STEP FORMULATION
   ============================================================================

   The goal was to translate a real‑world height map (land surface from the
   Dead Sea to Mount Everest) into an 8‑bit format (0‑255) where:
      * 0 is reserved for sea surface (water).
      * Land below sea level uses values 1‑10.
      * Land above sea level uses values 11‑255.
   The scaling had to be PROGRESSIVE, i.e. most detailed where land cover
   and population are densest, and increasingly compressed toward the extremes.

   --- Step 1: Split into two domains ---
   Below sea level (negative elevations) and above sea level (positive).
   The user specified that only 9‑10 values should be used for sub‑sea land
   because that area is extremely small in total land surface.

   --- Step 2: Define sub‑sea bands ---
   The user provided explicit elevation boundaries for below‑sea‑level bands:
     UNDER -400   →  band 1
     UP TO -300   →  band 2
     UP TO -250   →  band 3
     UP TO -200   →  band 4
     UP TO -150   →  band 5
     UP TO -100   →  band 6
     UP TO -50    →  band 7
     UP TO -20    →  band 8
     UP TO -7.8   →  band 9
     UP TO -0 (actually -1/32768) → band 10
   This places the most “detailed” part of the sub‑sea range near the coast,
   where shallow depressions (and some population) actually exist.

   --- Step 3: Above sea level – initial 235‑band table ---
   For positive elevations the user wanted exactly 235 bands (output values
   11‑245) that reflect the cumulative land area and population distribution.
   The process was:
      a) Model cumulative land fraction: L(H) = 1 - exp(-H/868.6)
      b) Model cumulative population fraction P(H) using a smooth curve
         that fits real‑world data (~50% below 200 m, 90% below 2000 m,
         then slowly saturating).
      c) Combine them: Combined(H) = (L(H) + P(H))/2.
      d) Assign levels proportionally to the combined fraction in each
         50‑m band (or later using a continuous cumulative curve).
      e) Convert to discrete bands. The user iteratively refined the table
         to avoid “dead zones” (where many bands got 0 levels) and to ensure
         a steady progression. The final 235‑band upper bounds (in metres)
         were manually adjusted and verified to give a smooth progressive
         mapping from 11 to 245.

   --- Step 4: Extend the positive bands ---
   Later the user added extra high‑altitude bands beyond the original
   7285 m limit, up to 8846 m. The new boundaries are:
     7386, 7466, 7556, 7666, 7796, 7946, 8126, 8336, 8576, 8846.
   This allows the very highest peaks (Everest = 8848 m) to still be mapped
   to the maximum value 255.

   --- Step 5: Exact zero‑crossing handling ---
   The user defined the exact transition between sub‑sea and above‑sea land
   as ±1/32768 metres (±0.0000305176 m). Any elevation strictly > 0 gets
   value ≥11; any elevation < 0 gets value ≤10; exact 0 remains water (0).

   --- Step 6: Final formula in code ---
   The mapping is implemented using two constexpr arrays containing the
   upper bounds of each band (ascending order). For a given elevation H:
      if H == 0 → value 0
      if H < 0 → std::ranges::lower_bound on the sub‑sea array → value 1‑10
      if H > 0 → std::ranges::lower_bound on the positive array → value 11‑255
   The lookup is O(log N) per pixel (binary search), but because the arrays
   are small (10 and 245 elements) it is extremely fast.

   This scheme faithfully reproduces the manually constructed progressive
   table, blending uniform band counting with realistic population/land
   density curves, and exactly matches the user’s requested output range.
   ============================================================================ */

// -----------------------------------------------------------------------------
// Numerical constants for the exact zero‑crossing
// -----------------------------------------------------------------------------
constexpr double neg_zero = -1.0 / 32768.0; // highest negative that is not 0
constexpr double pos_zero = 1.0 / 32768.0;  // lowest positive that is not 0

// -----------------------------------------------------------------------------
// Sub‑sea bands (NewBand 1 … 10) – upper bounds in ascending order.
// -----------------------------------------------------------------------------
constexpr std::array<double, 10> subsea_upper = {
    -400.0, -300.0, -250.0, -200.0, -150.0,
    -100.0, -50.0, -20.0, -7.8, neg_zero};

// -----------------------------------------------------------------------------
// Positive bands (NewBand 11 … 255) – upper bounds in ascending order.
// Original 235 bands + 10 new high‑altitude bands.
// -----------------------------------------------------------------------------
constexpr std::array<double, 245> positive_upper = {
    7.8, 15.5, 25.8, 36.2, 46.5, 54.3, 62.0, 69.8, 77.5, 85.3,
    93.0, 100.8, 108.5, 113.7, 118.8, 124.0, 129.2, 134.3, 139.5, 144.7,
    149.8, 155.0, 160.2, 165.3, 170.5, 174.9, 179.4, 183.8, 188.2, 192.6,
    197.1, 201.5, 205.9, 210.4, 214.8, 219.2, 223.6, 228.1, 232.5, 236.9,
    241.4, 245.8, 250.2, 254.6, 259.1, 263.5, 267.9, 272.4, 276.8, 281.2,
    285.6, 290.1, 294.5, 298.9, 303.4, 307.8, 312.2, 316.6, 321.1, 325.5,
    331.7, 337.9, 344.1, 350.3, 356.5, 362.7, 368.9, 375.1, 381.3, 387.5,
    393.7, 399.9, 406.1, 412.3, 418.5, 426.3, 434.0, 441.8, 449.5, 459.8,
    470.2, 480.5, 490.8, 501.2, 511.5, 521.8, 532.2, 542.5, 552.8, 563.2,
    573.5, 589.0, 604.5, 620.0, 635.5, 651.0, 666.5, 682.0, 697.5, 713.0,
    728.5, 744.0, 759.5, 790.5, 806.0, 821.5, 852.5, 868.0, 883.5, 914.5,
    945.5, 961.0, 976.5, 1007.5, 1038.5, 1069.5, 1085.0, 1100.5, 1131.5, 1162.5,
    1193.5, 1224.5, 1255.5, 1271.0, 1286.5, 1317.5, 1348.5, 1379.5, 1410.5, 1441.5,
    1472.5, 1503.5, 1534.5, 1565.5, 1596.5, 1627.5, 1658.5, 1689.5, 1720.5, 1751.5,
    1782.5, 1813.5, 1844.5, 1875.5, 1906.5, 1937.5, 1968.5, 1999.5, 2030.5, 2061.5,
    2108.0, 2170.0, 2232.0, 2294.0, 2356.0, 2418.0, 2480.0, 2542.0, 2604.0, 2666.0,
    2728.0, 2790.0, 2852.0, 2914.0, 2976.0, 3038.0, 3100.0, 3162.0, 3224.0, 3286.0,
    3348.0, 3410.0, 3472.0, 3534.0, 3596.0, 3658.0, 3720.0, 3782.0, 3844.0, 3906.0,
    3968.0, 4030.0, 4092.0, 4154.0, 4216.0, 4278.0, 4340.0, 4402.0, 4464.0, 4526.0,
    4588.0, 4650.0, 4712.0, 4774.0, 4836.0, 4898.0, 4960.0, 5022.0, 5084.0, 5146.0,
    5208.0, 5270.0, 5332.0, 5394.0, 5456.0, 5518.0, 5580.0, 5642.0, 5704.0, 5766.0,
    5828.0, 5890.0, 5952.0, 6014.0, 6076.0, 6138.0, 6200.0, 6262.0, 6324.0, 6386.0,
    6448.0, 6510.0, 6572.0, 6634.0, 6696.0, 6758.0, 6820.0, 6882.0, 6944.0, 7006.0,
    7068.0, 7130.0, 7192.0, 7254.0, 7316.0, 7386.0, 7466.0, 7556.0, 7666.0, 7796.0,
    7946.0, 8126.0, 8336.0, 8576.0, 8846.0};

// -----------------------------------------------------------------------------
// Compress a raw 16‑bit elevation (metres) to an 8‑bit value (0‑255)
// using the progressive band mapping described above.
// -----------------------------------------------------------------------------
inline uint8_t compress(int16_t raw)
{
    if (raw == 0)
        return 0; // water surface exactly

    if (raw < 0)
    {
        // sub‑sea land – find the first upper bound >= raw
        const auto it = std::ranges::lower_bound(subsea_upper, static_cast<double>(raw));
        const auto idx = std::distance(subsea_upper.begin(), it);
        return static_cast<uint8_t>(idx + 1); // 1 .. 10
    }

    // raw > 0 – above sea level
    if (raw >= 8846) // top of last band
        return 255;

    const auto it = std::ranges::lower_bound(positive_upper, static_cast<double>(raw));
    const auto idx = std::distance(positive_upper.begin(), it);
    return static_cast<uint8_t>(idx + 11); // 11 .. 255
}

// -----------------------------------------------------------------------------
// Main program – reads a 2:1 16‑bit height map, compresses it, writes an 8‑bit file.
// -----------------------------------------------------------------------------
int main()
{
    // Try to locate the project root (where the executable is located)
    fs::path exePath = fs::current_path();
    // If the exe is inside Executables/, go up one level to the project root
    if (exePath.filename() == "Executables")
        exePath = exePath.parent_path();

    // Input: the 2:1 16‑bit map created with gdalwarp
    fs::path inFile = exePath / "Input" / "GEBCO_2to1_16bit.bin";
    // Output: the 8‑bit compressed heightmap
    fs::path outFile = exePath / "Input" / "HeightMap.bin";

    std::cout << "Input:  " << inFile.string() << "\n";
    std::cout << "Output: " << outFile.string() << "\n";

    if (!fs::exists(inFile))
    {
        std::cerr << "ERROR: Input file not found!\n";
        std::cerr << "Please run this program from the project root, or place the file at the correct location.\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::ifstream fin(inFile, std::ios::binary);
    if (!fin)
    {
        std::cerr << "Cannot open " << inFile << "\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::ofstream fout(outFile, std::ios::binary);
    if (!fout)
    {
        std::cerr << "Cannot create " << outFile << "\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    constexpr size_t numPixels = static_cast<size_t>(WIDTH) * HEIGHT;
    std::cout << "Compressing " << numPixels << " pixels...\n";

    // Process row by row to keep memory low
    std::vector<int16_t> rowIn(WIDTH);
    std::vector<uint8_t> rowOut(WIDTH);

    for (int y = 0; y < HEIGHT; ++y)
    {
        fin.read(reinterpret_cast<char *>(rowIn.data()), WIDTH * sizeof(int16_t));
        if (!fin)
        {
            std::cerr << "Error reading row " << y << "\n";
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }
        for (int x = 0; x < WIDTH; ++x)
            rowOut[x] = compress(rowIn[x]);

        fout.write(reinterpret_cast<const char *>(rowOut.data()), WIDTH);
        if (y % 10000 == 0)
            std::cout << "Row " << y << " / " << HEIGHT << "\n";
    }
    std::cout << "Done! Output written to " << outFile << " (2 GiB)\n";
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}