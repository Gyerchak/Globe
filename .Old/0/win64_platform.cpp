/*
    Simple Win32 Game Window with Double-Buffered GDI Rendering
    ------------------------------------------------------------
    Creates a window, allocates two pixel buffers (internal 64-bit per pixel
    and display 32-bit per pixel), runs an animation that shifts colour
    components, converts the 64-bit buffer down to 32-bit, and draws it
    using StretchDIBits.

    Many of the structures at the top are placeholders for a larger game engine
    (sound, multithreading, hot-reloadable DLL game code). They are not used in
    the current loop but are included for future expansion.
*/

#include <windows.h>
#include <cstdint>

// Global flag controlling the main loop (set to false on window close)
bool running = true;

// ---------------------------------------------------------------------------
// Placeholder / future-use structures (not actively used in this specific demo)
// ---------------------------------------------------------------------------

// Holds nothing yet – a placeholder for a full renderer state.
struct Render_state
{
    int dummy;
};

// Describes a GDI-compatible off‑screen bitmap, storing pixel data and metadata.
struct Win32OffscreenBuffer
{
    BITMAPINFO bitmap_info; // Header describing pixel format, dimensions, etc.
    void *memory;           // Pointer to the raw pixel buffer
    int width;
    int height;
    int pitch; // Byte stride per row (often width * bytes per pixel)
};

// Contains the core Win32 handles and buffers needed for the application window.
struct Win32App
{
    HINSTANCE instance;
    HWND window;
    HDC device_context;
    Win32OffscreenBuffer offscreen_buffer;
    Render_state render_state;
};

// Simple 2D size struct for passing window dimensions.
struct win32_window_dimension
{
    int width;
    int height;
};

// Describes the desired sound output format and latency.
struct win32_sound_output
{
    int samples_per_second;
    int bytes_per_sample;
    int secondary_buffer_size;
    int latency_sample_count;
};

// Debug marker for associating a log message with system time and CPU cycle count.
struct win32_debug_time_marker
{
    DWORD output_debug_string_thread_id;
    DWORD output_debug_string_system_time;
    DWORD output_debug_string_cpu_cycle_count;
    char message[256];
};

// Holds a loaded game DLL and its timestamp, plus function pointers to its exported routines.
struct win32_game_code
{
    HMODULE game_code_dll;
    FILETIME dll_last_write_time;

    void (*update_and_render)(Render_state *render_state, int buffer_width, int buffer_height, void *buffer_memory);
    void (*get_sound_samples)(win32_sound_output *sound_output, int sample_count, int16_t *sample_buffer);
    void (*debug_frame_end)();
};

// Represents a memory‑mapped replay file.
struct win32_replay_buffer
{
    HANDLE file_handle;
    HANDLE memory_map;
    char *base_address;
};

// Global application state – holds paths, game code, replay buffers.
struct win32_state
{
    bool is_initialized;
    char exe_path[MAX_PATH];
    char *one_past_last_exe_path_slash; // pointer into exe_path for easy filename extraction
    win32_game_code game_code;
    win32_replay_buffer replay_buffers[4];
};

// Threading helpers (currently empty – placeholders for a job system)
struct win32_thread_info
{
    int placeholder;
};
struct win32_work_queue
{
};
struct win32_thread_context
{
    int placeholder;
};
struct win32_semaphore
{
    HANDLE semaphore_handle;
};
struct win32_mutex
{
    HANDLE mutex_handle;
};
struct win32_condition_variable
{
    CONDITION_VARIABLE condition_variable;
};
struct win32_thread
{
    HANDLE thread_handle;
    DWORD thread_id;
};
struct win32_thread_startup
{
    void (*callback)(void *thread_context);
    void *thread_context;
};
struct win32_queued_work
{
    void (*callback)(void *data);
    void *data;
};
struct win32_queue_entry
{
    win32_queued_work work;
    win32_queue_entry *next;
};
struct win32_queue
{
    win32_queue_entry *first;
    win32_queue_entry *last;
    int entry_count;
};
struct win32_thread_queue
{
    win32_queue queue;
    CRITICAL_SECTION queue_lock;
    HANDLE semaphore_handle;
};

// ================================================================
// Global buffers and their descriptors
// ================================================================

// Internal 64‑bit per‑pixel buffer (used for game logic; stores 16‑bit colour channels)
void *buffer_memory = nullptr;

// Display buffer – 32‑bit per‑pixel (8 bits per channel) ready for StretchDIBits
void *display_memory = nullptr;

int buffer_width = 0;
int buffer_height = 0;

// BITMAPINFO used for StretchDIBits (top‑down DIB with negative height)
static BITMAPINFO bitmap_info;
static BITMAPINFO buffer_bitmap_info; // identical copy (can be removed if not needed)

// ---------------------------------------------------------------------------
// Window callback – handles OS messages sent to the window
// ---------------------------------------------------------------------------
LRESULT CALLBACK window_callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    switch (uMsg)
    {
    case WM_CLOSE:
    case WM_DESTROY:
    {
        // Stop the main loop when the window is closed.
        running = false;
    }
    break;

    case WM_SIZE:
    {
        // The client area size changed – we must reallocate our buffers.

        RECT rect;
        GetClientRect(hwnd, &rect);
        buffer_width = rect.right - rect.left;
        buffer_height = rect.bottom - rect.top;

        // Calculate required memory size for both buffers.
        int64_t buffer_size = (int64_t)buffer_width * (int64_t)buffer_height * (int64_t)sizeof(uint64_t);
        int64_t display_size = (int64_t)buffer_width * (int64_t)buffer_height * (int64_t)sizeof(uint32_t);

        // Free previous buffers if they exist.
        if (buffer_memory)
            VirtualFree(buffer_memory, 0, MEM_RELEASE);
        if (display_memory)
            VirtualFree(display_memory, 0, MEM_RELEASE);

        // Allocate new buffers with read/write permissions.
        buffer_memory = VirtualAlloc(nullptr, (SIZE_T)buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        display_memory = VirtualAlloc(nullptr, (SIZE_T)display_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        // Set up the BITMAPINFO describing a 32‑bit top‑down bitmap.
        // biHeight negative = top‑down (first row is the top of the image).
        bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
        bitmap_info.bmiHeader.biWidth = buffer_width;
        bitmap_info.bmiHeader.biHeight = -buffer_height;
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;

        // Keep a copy (not strictly needed, but remains here for completeness).
        buffer_bitmap_info = bitmap_info;
    }
    break;

    default:
    {
        // Let Windows handle any other message.
        result = DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    break;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Application entry point (Unicode build)
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register a window class that defines the behaviour of our window.
    WNDCLASSW window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW; // Redraw on resize
    window_class.lpfnWndProc = window_callback;   // Our message handler
    window_class.hInstance = hInstance;
    window_class.lpszClassName = L"Game Window Class";

    RegisterClassW(&window_class);

    // Create the actual window (1920×1080 initial size, but the client area
    // will be smaller due to borders; WM_SIZE will give us the real size).
    HWND window = CreateWindowW(
        window_class.lpszClassName,
        L"game window ",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1920, 1080,
        nullptr, nullptr, hInstance, nullptr);

    // Get the device context for the whole window – we keep it for the lifetime.
    HDC hdc = GetDC(window);

    // ========== MAIN LOOP ==========
    while (running)
    {
        // Process all pending Windows messages.
        MSG message;
        while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        // If buffers are allocated (window has been sized at least once), update and render.
        if (buffer_memory && display_memory)
        {
            // --- Step 1: Convert the internal 64‑bit buffer to 32‑bit display buffer ---
            uint64_t *src = (uint64_t *)buffer_memory;
            uint32_t *dst = (uint32_t *)display_memory;
            int pixelCount = buffer_width * buffer_height;

            for (int i = 0; i < pixelCount; ++i)
            {
                uint64_t p = src[i];
                // The 64‑bit value is laid out as: [16b R][16b G][16b B]
                // (actual layout used in the animation: high word = R, middle = G, low = B)
                uint32_t b16 = (uint32_t)(p & 0xFFFF);
                uint32_t g16 = (uint32_t)((p >> 16) & 0xFFFF);
                uint32_t r16 = (uint32_t)((p >> 32) & 0xFFFF);

                // Down‑sample 16‑bit channels to 8‑bit (just keeping the upper byte).
                uint32_t r8 = r16 >> 8;
                uint32_t g8 = g16 >> 8;
                uint32_t b8 = b16 >> 8;

                // Combine into a 32‑bit pixel (ARGB format, alpha left as 0 – opaque).
                dst[i] = (r8 << 16) | (g8 << 8) | b8;
            }

            // --- Step 2: Draw the display buffer using GDI (first call) ---
            StretchDIBits(hdc,
                          0, 0, buffer_width, buffer_height, // Destination rect (client area)
                          0, 0, buffer_width, buffer_height, // Source rect (whole buffer)
                          display_memory, &bitmap_info,
                          DIB_RGB_COLORS, SRCCOPY);

            // --- Step 3: Animate the internal 64‑bit buffer in place ---
            unsigned char *pixel = (unsigned char *)buffer_memory;
            for (int y = 0; y < buffer_height; ++y)
            {
                for (int x = 0; x < buffer_width; ++x)
                {
                    int index = (y * buffer_width + x) * 8; // 8 bytes per pixel in buffer_memory

                    // Extract the three 16‑bit colour components.
                    // Byte order assumed: [b_low][b_high][g_low][g_high][r_low][r_high]
                    uint16_t r = (uint16_t)(pixel[index + 4] << 8 | pixel[index + 5]);
                    uint16_t g = (uint16_t)(pixel[index + 2] << 8 | pixel[index + 3]);
                    uint16_t b = (uint16_t)(pixel[index + 0] << 8 | pixel[index + 1]);

                    // Shift the colours by a small amount (simple animation).
                    r = (r + 1) % 65536;
                    g = (g + 2) % 65536;
                    b = (b + 3) % 65536;

                    // Write the updated colours back into the buffer.
                    pixel[index + 4] = r >> 8;   // Red high byte
                    pixel[index + 5] = r & 0xFF; // Red low byte
                    pixel[index + 2] = g >> 8;
                    pixel[index + 3] = g & 0xFF;
                    pixel[index + 0] = b >> 8;
                    pixel[index + 1] = b & 0xFF;
                }
            }

            // --- Step 4: Draw the buffer again (now with the new frame) ---
            StretchDIBits(hdc,
                          0, 0, buffer_width, buffer_height,
                          0, 0, buffer_width, buffer_height,
                          display_memory, &bitmap_info,
                          DIB_RGB_COLORS, SRCCOPY);
        }
    }

    // Cleanup: release the device context and exit.
    ReleaseDC(window, hdc);
    return 0;
}

/*
===========================================================================
BUILD INSTRUCTIONS (MinGW-w64 / Git Bash)
===========================================================================
Open Git Bash in the project directory and run:

   g++ -std=c++20 -O2 -static -municode -o win64_game.exe win64_game.cpp -lgdi32

   Explanation of flags:
     -std=c++20  : Use the C++20 standard.
     -O2         : Optimise for speed and size.
     -static     : Link everything into a single .exe – no external DLLs needed.
     -municode   : Use the Unicode entry point (wWinMain). REQUIRED.
     -lgdi32     : Link GDI32 library (StretchDIBits, painting).

   If you prefer a dynamic build (smaller .exe but requires MinGW DLLs),
   remove the -static flag.

===========================================================================
WHAT THIS PROGRAM DOES
===========================================================================
A simple Win32 window that creates two off‑screen buffers:
   1. A 64‑bit‑per‑pixel internal buffer (RGB channels stored as 16‑bit values).
   2. A 32‑bit‑per‑pixel display buffer (RGB channels down‑sampled to 8 bits).

Every frame it:
   - Converts the 64‑bit buffer down to 32‑bit.
   - Draws the 32‑bit buffer to the window using StretchDIBits (first draw).
   - Animates the 64‑bit buffer by incrementing each colour channel.
   - Draws the now‑updated buffer again (second draw).

The many placeholder structs at the top are scaffolding for a full game
engine (hot‑reload DLL, multithreading, sound). They are not used by the
animation loop and can be safely ignored.

Close the window to exit.
===========================================================================
*/