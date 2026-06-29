// HexPngCreator.cpp
// C++20 program that generates PNG images of white hexagons on black background.
// Sizes: 8x8, 10x10, ..., up to 518x518 (256 images total).
// Output is placed in <project-root>/Output/Hex/   (always next to the Executables folder).

#include <iostream>
#include <filesystem>
#include <cmath>
#include <vector>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../.dependencies/stb_image_write.h"

namespace fs = std::filesystem;

// Pointy-topped hexagon test (centre cx,cy, radius R)
bool hexagon_contains(double px, double py, double cx, double cy, double R)
{
    double dy = std::abs(py - cy);
    if (dy > R)
        return false;
    if (dy <= R / 2.0)
        return std::abs(px - cx) <= (std::sqrt(3.0) / 2.0) * R;
    else
    {
        double half_width = std::sqrt(3.0) * (R - dy);
        return std::abs(px - cx) <= half_width;
    }
}

int main(int argc, char *argv[])
{
    constexpr int total_images = 256;
    constexpr int start_size = 8;

    // Determine the project root: the directory *above* the Executables folder.
    // Fallback to current working directory if executable path cannot be resolved.
    fs::path exe_path;
    try
    {
        exe_path = fs::canonical(argv[0]); // fully resolved path of the .exe
    }
    catch (...)
    {
        exe_path = fs::current_path(); // fallback
    }
    fs::path project_root = exe_path.parent_path().parent_path(); // go up one level (Executables -> project root)
    if (!fs::exists(project_root))
    {
        // Fallback: use working directory
        project_root = fs::current_path();
    }

    fs::path out_dir = project_root / "Output" / "Hex";

    // Create output directories
    try
    {
        fs::create_directories(out_dir);
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error creating directories: " << e.what() << '\n';
        return 1;
    }

    std::cout << "Generating " << total_images << " PNG images...\n";
    std::cout << "Output directory: " << fs::absolute(out_dir) << '\n';

    for (int k = 0; k < total_images; ++k)
    {
        int N = start_size + 2 * k; // 8, 10, ..., 518
        double R = (N - 1) / 2.0;
        double cx = R, cy = R;

        std::vector<unsigned char> image(N * N * 3, 0); // all black (0)

        for (int y = 0; y < N; ++y)
        {
            for (int x = 0; x < N; ++x)
            {
                double px = x + 0.5;
                double py = y + 0.5;
                if (hexagon_contains(px, py, cx, cy, R))
                {
                    size_t idx = 3 * (y * N + x);
                    image[idx + 0] = 255; // R
                    image[idx + 1] = 255; // G
                    image[idx + 2] = 255; // B
                }
            }
        }

        std::string filename = std::to_string(N) + "x" + std::to_string(N) + ".png";
        fs::path filepath = out_dir / filename;

        if (!stbi_write_png(filepath.string().c_str(), N, N, 3, image.data(), N * 3))
        {
            std::cerr << "Failed to write " << filepath << '\n';
            return 1;
        }
    }

    std::cout << "Done.\n";
    return 0;
}