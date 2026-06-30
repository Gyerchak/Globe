// DivideHeightMap.cpp
// Splits Input/HeightMap.Bin (65536×32768, 8‑bit) into 162 tiles.
// File names: XY_AB.bin
//   X = column letter A..R (18 columns)
//   Y = row digit 0..8
//   A = old X value (-9..-1, 1..9) with:
//         -1 → "[-1]"
//          1 → "[+1]"
//         others unchanged
//   B = old Y value (4..-4) with 0 → "[0]"
// Tiles saved to Output/DividedHeightMap/

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string>
#include <cstdint>
#include <algorithm>

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    // ---------- Project root detection ----------
    fs::path exe_path;
    try
    {
        exe_path = fs::canonical(argv[0]);
    }
    catch (...)
    {
        exe_path = fs::current_path();
    }
    fs::path project_root = exe_path.parent_path().parent_path();
    if (!fs::exists(project_root))
        project_root = fs::current_path();

    fs::path input_path = project_root / "Input" / "HeightMap.Bin";
    fs::path output_dir = project_root / "Output" / "DividedHeightMap";

    if (!fs::exists(input_path))
    {
        std::cerr << "Error: input file not found: " << fs::absolute(input_path) << '\n';
        return 1;
    }
    try
    {
        fs::create_directories(output_dir);
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error creating output directory: " << e.what() << '\n';
        return 1;
    }

    // ---------- Map and tile grid ----------
    constexpr int W = 65536, H = 32768;
    constexpr int num_cols = 18; // X: letters A..R
    constexpr int num_rows = 9;  // Y: digits 0..8

    // Column widths (distribute remainder)
    int col_widths[num_cols];
    int x_start[num_cols];
    {
        int base_w = W / num_cols;
        int rem_w = W % num_cols;
        int acc = 0;
        for (int c = 0; c < num_cols; ++c)
        {
            col_widths[c] = base_w + (c < rem_w ? 1 : 0);
            x_start[c] = acc;
            acc += col_widths[c];
        }
    }

    // Row heights (distribute remainder)
    int row_heights[num_rows];
    int y_start[num_rows];
    {
        int base_h = H / num_rows;
        int rem_h = H % num_rows;
        int acc = 0;
        for (int r = 0; r < num_rows; ++r)
        {
            row_heights[r] = base_h + (r < rem_h ? 1 : 0);
            y_start[r] = acc;
            acc += row_heights[r];
        }
    }

    // ---------- Open input ----------
    std::ifstream fin(input_path, std::ios::binary);
    if (!fin)
    {
        std::cerr << "Error opening input file.\n";
        return 1;
    }

    // ---------- Process strips ----------
    for (int tile_row = 0; tile_row < num_rows; ++tile_row)
    {
        int strip_y = y_start[tile_row];
        int strip_h = row_heights[tile_row];

        // Read whole strip
        std::vector<uint8_t> strip(static_cast<size_t>(W) * strip_h);
        fin.seekg(static_cast<std::streamoff>(strip_y) * W, std::ios::beg);
        if (!fin.read(reinterpret_cast<char *>(strip.data()), strip.size()))
        {
            std::cerr << "Error reading input file at row " << strip_y << '\n';
            return 1;
        }

        for (int tile_col = 0; tile_col < num_cols; ++tile_col)
        {
            // --- Build new‑style filename components ---
            // X: column letter
            char X_char = static_cast<char>('A' + tile_col); // A..R
            // Y: row digit (0..8)
            char Y_char = static_cast<char>('0' + tile_row);

            // Old X (original X): -9..-1 for cols 0..8, 1..9 for cols 9..17
            int oldX;
            if (tile_col < 9)
                oldX = tile_col - 9; // -9..-1
            else
                oldX = tile_col - 8; // 1..9

            // Old Y (original Y): 4 down to -4
            int oldY = 4 - tile_row; // 4..-4

            // A string: special brackets only for -1 and +1
            std::string A_str;
            if (oldX == -1)
            {
                A_str = "[-1]";
            }
            else if (oldX == 1)
            {
                A_str = "[+1]";
            }
            else
            {
                A_str = std::to_string(oldX);
            }

            // B string: 0 → "[0]", others unchanged
            std::string B_str;
            if (oldY == 0)
            {
                B_str = "[0]";
            }
            else
            {
                B_str = std::to_string(oldY);
            }

            // Compose filename: XY_AB.bin
            std::string fname;
            fname += X_char;
            fname += Y_char;
            fname += '_';
            fname += A_str;
            fname += B_str;
            fname += ".bin";

            // --- Extract tile from strip ---
            int tw = col_widths[tile_col];
            int th = strip_h; // all tiles in row share height
            int sx = x_start[tile_col];

            std::vector<uint8_t> tile(static_cast<size_t>(tw) * th);
            for (int r = 0; r < th; ++r)
            {
                const uint8_t *src_row = strip.data() + static_cast<size_t>(r) * W + sx;
                std::copy(src_row, src_row + tw, tile.data() + static_cast<size_t>(r) * tw);
            }

            // Write tile
            fs::path out_file = output_dir / fname;
            std::ofstream fout(out_file, std::ios::binary);
            if (!fout)
            {
                std::cerr << "Error writing " << out_file << '\n';
                return 1;
            }
            fout.write(reinterpret_cast<const char *>(tile.data()), tile.size());
        }
    }

    std::cout << "Tiles saved to:\n"
              << fs::absolute(output_dir) << '\n';
    return 0;
}