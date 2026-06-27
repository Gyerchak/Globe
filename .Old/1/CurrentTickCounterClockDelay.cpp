// CurrentTickCounterClockDelay.cpp
// Compile: g++ -std=c++20 -static -o CurrentTickCounterClockDelay.exe CurrentTickCounterClockDelay.cpp

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdint>
#include <filesystem>

int main()
{
    std::cout << "CurrentTickCounterClockDelay (Speed Controller)\n";
    std::cout << "Enter a number (0-15) and press Enter to change simulation speed.\n";
    std::cout << "Mapping: 0=no delay, 1=1µs,2=5µs,3=10µs,4=50µs,5=100µs,6=500µs,\n";
    std::cout << "7=1ms,8=10ms,9=100ms,10=500ms,11=1s,12=10s,13=1min,14=10min,15=pause.\n";
    std::cout << "Type 'q' to quit.\n\n";

    // Ensure directory exists
    std::filesystem::path delayFile = "GameData/Line/CurrentTickCounterClockDelay.bin";
    std::filesystem::create_directories(delayFile.parent_path());

    // Optional: read current clock tick for display
    std::filesystem::path clockFile = "GameData/Line/CurrentTickCounterClock.bin";
    auto lastClockTime = std::chrono::steady_clock::now();

    std::string line;
    while (true)
    {
        std::cout << "Break (0-15) > ";
        std::getline(std::cin, line);
        if (line == "q" || line == "Q")
            break;

        int val;
        try
        {
            val = std::stoi(line);
        }
        catch (...)
        {
            std::cout << "Invalid number.\n";
            continue;
        }
        if (val < 0 || val > 15)
        {
            std::cout << "Value must be 0-15.\n";
            continue;
        }

        // Write break value to delay file (1 byte)
        uint8_t breakVal = static_cast<uint8_t>(val);
        std::ofstream f(delayFile, std::ios::binary);
        if (f.is_open())
        {
            f.write(reinterpret_cast<const char *>(&breakVal), sizeof(breakVal));
            std::cout << "Speed changed to " << val << "\n";
        }
        else
        {
            std::cout << "Warning: could not write delay file.\n";
        }

        // Optional: read and display current tick from clock file (if it exists)
        if (std::filesystem::exists(clockFile))
        {
            // To avoid spamming, read at most once per second
            auto now = std::chrono::steady_clock::now();
            if (now - lastClockTime > std::chrono::seconds(1))
            {
                lastClockTime = now;
                std::ifstream cf(clockFile, std::ios::binary);
                if (cf)
                {
                    uint32_t tick;
                    cf.read(reinterpret_cast<char *>(&tick), sizeof(tick));
                    if (cf.gcount() == sizeof(tick))
                    {
                        // Convert tick to date/time (10 minutes per tick)
                        uint64_t totalMinutes = static_cast<uint64_t>(tick) * 10;
                        uint64_t totalHours = totalMinutes / 60;
                        uint8_t hour = totalHours % 24;
                        uint64_t totalDays = totalHours / 24;
                        uint8_t day = totalDays % 30;
                        uint64_t totalMonths = totalDays / 30;
                        uint8_t month = totalMonths % 12;
                        uint16_t year = totalMonths / 12;
                        int displayedYear = static_cast<int>(year) - 4500;
                        int minute = (tick * 10) % 60;
                        std::cout << " (Clock shows: " << displayedYear << "-" << (month + 1) << "-" << (day + 1)
                                  << " " << (int)hour << ":" << minute << ")\n";
                    }
                }
            }
        }
    }
    return 0;
}