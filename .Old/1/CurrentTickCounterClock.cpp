// CurrentTickCounterClock.cpp
// Compile: g++ -std=c++20 -static -o CurrentTickCounterClock.exe CurrentTickCounterClock.cpp

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <windows.h>

// --- Convert tick (10 minutes each) to time components ---
void ticksToTime(uint32_t tick, uint8_t &hour, uint8_t &day, uint8_t &month, uint16_t &year)
{
    uint64_t totalMinutes = static_cast<uint64_t>(tick) * 10;
    const uint64_t minutesPerHour = 60;
    const uint64_t hoursPerDay = 24;
    const uint64_t daysPerMonth = 30;
    const uint64_t monthsPerYear = 12;

    uint64_t totalHours = totalMinutes / minutesPerHour;
    hour = totalHours % hoursPerDay;
    uint64_t totalDays = totalHours / hoursPerDay;
    day = totalDays % daysPerMonth;
    uint64_t totalMonths = totalDays / daysPerMonth;
    month = totalMonths % monthsPerYear;
    year = totalMonths / monthsPerYear;
}

// --- Map break value (0-15) to microseconds; 15 = pause ---
uint64_t mapBreakToDelayUs(uint8_t breakVal)
{
    switch (breakVal)
    {
    case 0:
        return 0;
    case 1:
        return 1; // 1 µs
    case 2:
        return 5; // 5 µs
    case 3:
        return 10; // 10 µs
    case 4:
        return 50; // 50 µs
    case 5:
        return 100; // 100 µs
    case 6:
        return 500; // 500 µs
    case 7:
        return 1000; // 1 ms
    case 8:
        return 10000; // 10 ms
    case 9:
        return 100000; // 100 ms
    case 10:
        return 500000; // 500 ms
    case 11:
        return 1000000; // 1 s
    case 12:
        return 10000000; // 10 s
    case 13:
        return 60000000; // 1 min
    case 14:
        return 600000000; // 10 min
    default:
        return 0; // breakVal == 15 (pause)
    }
}

// --- Interruptible delay: checks break file each step ---
void interruptibleDelay(uint64_t totalUs, const std::filesystem::path &delayFile, uint8_t originalBreak, std::atomic<bool> &running)
{
    if (totalUs == 0)
        return;
    const uint64_t stepUs = 10000; // 10 ms steps
    while (totalUs > 0 && running)
    {
        // Check if break value has changed (by reading file)
        uint8_t currentBreak = 15;
        std::ifstream f(delayFile, std::ios::binary);
        if (f.is_open())
        {
            f.read(reinterpret_cast<char *>(&currentBreak), sizeof(currentBreak));
        }
        if (currentBreak != originalBreak)
            return;
        uint64_t sleepUs = (totalUs > stepUs) ? stepUs : totalUs;
        if (sleepUs < 1000)
        {
            auto start = std::chrono::high_resolution_clock::now();
            auto end = start + std::chrono::microseconds(sleepUs);
            while (std::chrono::high_resolution_clock::now() < end)
            {
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
        }
        totalUs -= sleepUs;
    }
}

// --- Console positioning ---
void gotoxy(int x, int y)
{
    COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

int main()
{
    // Paths
    std::filesystem::path clockFile = "GameData/Line/CurrentTickCounterClock.bin";
    std::filesystem::path delayFile = "GameData/Line/CurrentTickCounterClockDelay.bin";
    std::filesystem::create_directories(clockFile.parent_path());

    // Default break = 15 (pause) if delay file missing
    uint8_t breakVal = 15;

    std::cout << "CurrentTickCounterClock started. Each tick = 10 minutes.\n";
    std::cout << "Reads delay from: " << delayFile << "\n";
    std::cout << "Writes tick to:   " << clockFile << "\n";
    std::cout << "Use CurrentTickCounterClockDelay.exe to change speed.\n";
    std::cout << "Type 0-15 in that program to adjust simulation speed.\n\n";

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    int statusRow = csbi.dwCursorPosition.Y;
    int inputRow = statusRow + 1; // not used for input now, but for layout

    std::atomic<bool> running = true;
    uint32_t tick = 0;
    bool wasPaused = false;

    while (running)
    {
        // --- Read current break from delay file ---
        std::ifstream f(delayFile, std::ios::binary);
        if (f.is_open())
        {
            f.read(reinterpret_cast<char *>(&breakVal), sizeof(breakVal));
        }
        else
        {
            // If file missing, default to pause
            breakVal = 15;
        }

        // --- Pause handling (break == 15) ---
        if (breakVal == 15)
        {
            if (!wasPaused)
            {
                gotoxy(0, statusRow);
                std::cout << "[PAUSED] Use delay controller to set 0-14.               ";
                wasPaused = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue; // do not advance tick
        }
        if (wasPaused)
        {
            gotoxy(0, statusRow);
            std::cout << std::string(60, ' ');
            wasPaused = false;
        }

        uint64_t delayUs = mapBreakToDelayUs(breakVal);

        // --- Compute time from tick ---
        uint8_t hour, day, month;
        uint16_t year;
        ticksToTime(tick, hour, day, month, year);

        int displayedYear = static_cast<int>(year) - 4500;
        int displayedMonth = month + 1;
        int displayedDay = day + 1;
        int minute = (tick * 10) % 60;

        // --- Write current tick to clock file (4 bytes little-endian) ---
        std::ofstream cf(clockFile, std::ios::binary);
        if (cf.is_open())
        {
            cf.write(reinterpret_cast<const char *>(&tick), sizeof(tick));
        }

        // --- Format delay string for display ---
        std::string delayStr;
        if (delayUs >= 1000000)
        {
            if (delayUs >= 60000000)
                delayStr = std::to_string(delayUs / 60000000) + " min";
            else
                delayStr = std::to_string(delayUs / 1000000) + " s";
        }
        else if (delayUs >= 1000)
        {
            delayStr = std::to_string(delayUs / 1000) + " ms";
        }
        else if (delayUs > 0)
        {
            delayStr = std::to_string(delayUs) + " µs";
        }
        else
        {
            delayStr = "0";
        }

        gotoxy(0, statusRow);
        std::cout << "Tick: " << tick
                  << "  Date: " << displayedYear << "-" << displayedMonth << "-" << displayedDay
                  << "  Time: " << (int)hour << ":" << minute
                  << "  (break=" << (int)breakVal << " -> " << delayStr << ")" << std::flush;

        // --- Delay with interruptibility (checks break file every 10ms) ---
        if (delayUs > 0)
        {
            interruptibleDelay(delayUs, delayFile, breakVal, running);
        }

        tick++; // 32-bit wrap is automatic
    }

    return 0;
}