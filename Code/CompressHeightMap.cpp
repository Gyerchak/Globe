// CompressHeightMap.cpp
// Location: Code/CompressHeightMap.cpp
// Compile: g++ -std=c++20 -static Code/CompressHeightMap.cpp -o Executables/CompressHeightMap.exe
// Run: ./Executables/CompressHeightMap.exe

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 65536;
constexpr double GAMMA = 0.5; // <1 expands lower values; adjust 0.3–0.7

// Map a raw 16-bit elevation (metres) to 0-255 using the non-linear scheme.
uint8_t compress(int16_t raw)
{
    if (raw <= 0)
        return 0;

    // 1-50 m: preserve full detail (linear)
    if (raw <= 50)
        return static_cast<uint8_t>(raw); // 1..50

    // Helper lambda: apply power curve within [inMin, inMax] -> [outMin, outMax]
    auto map_bucket = [&](int inMin, int inMax, int outMin, int outMax) -> uint8_t
    {
        double frac = (static_cast<double>(raw) - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    };

    if (raw <= 100)
        return map_bucket(51, 100, 51, 75);
    if (raw <= 200)
        return map_bucket(101, 200, 76, 100);
    if (raw <= 400)
        return map_bucket(201, 400, 101, 125);
    if (raw <= 800)
        return map_bucket(401, 800, 126, 150);
    if (raw <= 1600)
        return map_bucket(801, 1600, 151, 175);
    if (raw <= 3200)
        return map_bucket(1601, 3200, 176, 200);
    if (raw <= 6400)
        return map_bucket(3201, 6400, 201, 225);

    // Highest band: 6401..9000 m (Everest ~8848 m, with a little buffer)
    constexpr int inMin = 6401, inMax = 9000;
    constexpr int outMin = 226, outMax = 255;
    double frac = (static_cast<double>(raw) - inMin) / (inMax - inMin);
    if (frac > 1.0)
        frac = 1.0;
    double curved = std::pow(frac, GAMMA);
    return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
}

int main()
{
    // Input: the 16-bit square map created with gdalwarp
    const std::string inFile = "Input/GEBCO16bitSquare16bit.bin";

    // Output: the 8-bit compressed heightmap
    const std::string outFile = "Input/HeightMap.bin";

    std::ifstream fin(inFile, std::ios::binary);
    if (!fin)
    {
        std::cerr << "Cannot open " << inFile << "\n";
        return 1;
    }

    std::ofstream fout(outFile, std::ios::binary);
    if (!fout)
    {
        std::cerr << "Cannot create " << outFile << "\n";
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
            return 1;
        }
        for (int x = 0; x < WIDTH; ++x)
            rowOut[x] = compress(rowIn[x]);

        fout.write(reinterpret_cast<const char *>(rowOut.data()), WIDTH);
        if (y % 10000 == 0)
            std::cout << "Row " << y << " / " << HEIGHT << "\n";
    }
    std::cout << "Done. Output: " << outFile << " (4 GiB)\n";
    return 0;
}