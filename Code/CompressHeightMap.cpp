// CompressHeightmap.cpp
// Compile: g++ -std=c++20 -static CompressHeightmap.cpp -o CompressHeightmap.exe
// Run: ./CompressHeightmap.exe

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 65536;
constexpr double GAMMA = 0.5; // <1 expands lower values; adjust to taste (0.3–0.6)

// Compress a raw 16-bit elevation value into 0-255
uint8_t compress(int16_t raw)
{
    if (raw <= 0)
        return 0;

    // 0 is already handled; 1..50 maps linearly 1..50
    if (raw <= 50)
    {
        return static_cast<uint8_t>(raw); // 1..50
    }

    const double rawd = static_cast<double>(raw);

    // Each bucket: input range [inMin, inMax], output range [outMin, outMax]
    if (raw <= 100)
    {
        // 51..100 → 51..75  (25 values)
        constexpr int inMin = 51, inMax = 100;
        constexpr int outMin = 51, outMax = 75;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    if (raw <= 200)
    {
        // 101..200 → 76..100
        constexpr int inMin = 101, inMax = 200;
        constexpr int outMin = 76, outMax = 100;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    if (raw <= 400)
    {
        // 201..400 → 101..125
        constexpr int inMin = 201, inMax = 400;
        constexpr int outMin = 101, outMax = 125;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    if (raw <= 800)
    {
        // 401..800 → 126..150
        constexpr int inMin = 401, inMax = 800;
        constexpr int outMin = 126, outMax = 150;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    if (raw <= 1600)
    {
        // 801..1600 → 151..175
        constexpr int inMin = 801, inMax = 1600;
        constexpr int outMin = 151, outMax = 175;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    if (raw <= 3200)
    {
        // 1601..3200 → 176..200
        constexpr int inMin = 1601, inMax = 3200;
        constexpr int outMin = 176, outMax = 200;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    if (raw <= 6400)
    {
        // 3201..6400 → 201..225
        constexpr int inMin = 3201, inMax = 6400;
        constexpr int outMin = 201, outMax = 225;
        double frac = (rawd - inMin) / (inMax - inMin);
        double curved = std::pow(frac, GAMMA);
        return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
    }
    // 6401..highest (we set 9000 as practical max) → 226..255
    constexpr int inMin = 6401, inMax = 9000; // Mount Everest ~8848, plus a buffer
    constexpr int outMin = 226, outMax = 255;
    double frac = (rawd - inMin) / (inMax - inMin);
    // Clamp fraction to [0,1] for values above 9000
    if (frac > 1.0)
        frac = 1.0;
    double curved = std::pow(frac, GAMMA);
    return static_cast<uint8_t>(outMin + std::round((outMax - outMin) * curved));
}

int main()
{
    const std::string inFile = "Input/GEBCO_smooth.bin";
    const std::string outFile = "Input/GEBCO_65536Square.bin";

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
    std::cout << "Reading 16-bit data (" << numPixels << " pixels)...\n";

    // Process row by row to limit memory usage
    std::vector<int16_t> row(WIDTH);
    std::vector<uint8_t> outRow(WIDTH);

    for (int y = 0; y < HEIGHT; ++y)
    {
        fin.read(reinterpret_cast<char *>(row.data()), WIDTH * sizeof(int16_t));
        if (!fin)
        {
            std::cerr << "Error reading row " << y << "\n";
            return 1;
        }
        for (int x = 0; x < WIDTH; ++x)
        {
            outRow[x] = compress(row[x]);
        }
        fout.write(reinterpret_cast<const char *>(outRow.data()), WIDTH);
        if (y % 10000 == 0)
        {
            std::cout << "Processed " << y << " rows...\n";
        }
    }
    std::cout << "Done. Output written to " << outFile << "\n";
    return 0;
}