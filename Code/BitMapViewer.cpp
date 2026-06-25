// BitMapViewer.cpp
// Location: Code/BitMapViewer.cpp
// Compile: g++ -std=c++20 -I SourceCode -static Code/BitMapViewer.cpp -o Executables/BitMapViewer.exe -lglew32 -lglfw3 -lopengl32 -lgdi32
// Run: ./Executables/BitMapViewer.exe [path\to\file.bin]

#define GLEW_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

// ------------------------------------------------------------------
// Config
// ------------------------------------------------------------------
constexpr int SRC_WIDTH = 65536;
constexpr int SRC_HEIGHT = 65536;
constexpr int VIEW_SIZE = 8192; // display & PNG resolution
constexpr int WINDOW_W = 1024;
constexpr int WINDOW_H = 1024;

// ------------------------------------------------------------------
// Colour ramp for heightmap mode
// ------------------------------------------------------------------
struct Color
{
    uint8_t r, g, b;
};

Color heightToColor(uint8_t h)
{
    // 0 → blue, then green → yellow → orange → red → purple → white
    struct Stop
    {
        float pos;
        uint8_t r, g, b;
    };
    constexpr Stop stops[] = {
        {0.0f, 0, 0, 255},
        {0.15f, 0, 128, 0},
        {0.35f, 255, 255, 0},
        {0.55f, 255, 165, 0},
        {0.75f, 255, 0, 0},
        {0.90f, 128, 0, 128},
        {1.0f, 255, 255, 255}};
    constexpr int n = sizeof(stops) / sizeof(stops[0]);

    float t = h / 255.0f;
    // find segment
    int i = 0;
    while (i < n - 1 && t > stops[i + 1].pos)
        ++i;
    float segLen = stops[i + 1].pos - stops[i].pos;
    float local = (t - stops[i].pos) / segLen;
    if (local < 0)
        local = 0;
    if (local > 1)
        local = 1;

    return {
        static_cast<uint8_t>(stops[i].r + (stops[i + 1].r - stops[i].r) * local),
        static_cast<uint8_t>(stops[i].g + (stops[i + 1].g - stops[i].g) * local),
        static_cast<uint8_t>(stops[i].b + (stops[i + 1].b - stops[i].b) * local)};
}

// ------------------------------------------------------------------
// Load raw 8-bit heightmap
// ------------------------------------------------------------------
std::vector<uint8_t> loadHeightMap(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        std::cerr << "Cannot open " << path << '\n';
        return {};
    }
    const size_t total = static_cast<size_t>(SRC_WIDTH) * SRC_HEIGHT;
    std::vector<uint8_t> data(total);
    f.read(reinterpret_cast<char *>(data.data()), total);
    if (!f)
    {
        std::cerr << "Error reading file (expected " << total << " bytes)\n";
        return {};
    }
    return data;
}

// ------------------------------------------------------------------
// Downscale + colour → RGB array
// ------------------------------------------------------------------
std::vector<uint8_t> createRGB(const std::vector<uint8_t> &src, int viewSize)
{
    std::vector<uint8_t> rgb(viewSize * viewSize * 3);
    for (int y = 0; y < viewSize; ++y)
    {
        int srcY = static_cast<int>(static_cast<long long>(y) * SRC_HEIGHT / viewSize);
        for (int x = 0; x < viewSize; ++x)
        {
            int srcX = static_cast<int>(static_cast<long long>(x) * SRC_WIDTH / viewSize);
            uint8_t h = src[srcY * SRC_WIDTH + srcX];
            Color c = heightToColor(h);
            size_t idx = (static_cast<size_t>(y) * viewSize + x) * 3;
            rgb[idx + 0] = c.r;
            rgb[idx + 1] = c.g;
            rgb[idx + 2] = c.b;
        }
    }
    return rgb;
}

// ------------------------------------------------------------------
// Save PNG
// ------------------------------------------------------------------
void savePNG(const std::string &filename,
             const std::vector<uint8_t> &rgb, int w, int h)
{
    if (!stbi_write_png(filename.c_str(), w, h, 3, rgb.data(), w * 3))
        std::cerr << "Failed to write PNG: " << filename << '\n';
    else
        std::cout << "Saved " << filename << " (" << w << 'x' << h << ")\n";
}

// ------------------------------------------------------------------
// GL callbacks & helpers
// ------------------------------------------------------------------
static GLuint loadTexture(const std::vector<uint8_t> &rgb, int w, int h)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

static GLuint compileShader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

int main(int argc, char **argv)
{
    // Default input file
    std::string inputPath = "Input/HeightMap.bin";
    if (argc > 1)
        inputPath = argv[1];

    std::cout << "Loading heightmap: " << inputPath << '\n';
    auto raw = loadHeightMap(inputPath);
    if (raw.empty())
        return 1;

    // Build RGB data at view resolution
    std::cout << "Generating colour texture (" << VIEW_SIZE << 'x' << VIEW_SIZE << ")...\n";
    auto rgb = createRGB(raw, VIEW_SIZE);
    raw.clear(); // free 4 GB

    // GLFW window
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *win = glfwCreateWindow(WINDOW_W, WINDOW_H, "BitMapViewer", nullptr, nullptr);
    if (!win)
        return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
        return 1;

    // Upload texture
    GLuint tex = loadTexture(rgb, VIEW_SIZE, VIEW_SIZE);

    // Fullscreen quad shader
    const char *vsSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aTex;
out vec2 vTex;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTex = aTex;
}
)";
    const char *fsSrc = R"(
#version 330 core
in vec2 vTex;
out vec4 FragColor;
uniform sampler2D uTex;
void main() {
    FragColor = texture(uTex, vTex);
}
)";

    GLuint prog = glCreateProgram();
    glAttachShader(prog, compileShader(GL_VERTEX_SHADER, vsSrc));
    glAttachShader(prog, compileShader(GL_FRAGMENT_SHADER, fsSrc));
    glLinkProgram(prog);
    glUseProgram(prog);

    float quad[] = {
        // pos       tex
        -1,
        -1,
        0,
        1,
        1,
        -1,
        1,
        1,
        1,
        1,
        1,
        0,
        -1,
        -1,
        0,
        1,
        1,
        1,
        1,
        0,
        -1,
        1,
        0,
        0,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Mode info
    std::string modeName = "heightmap";
    std::string outputDir = "Output";
    fs::create_directories(outputDir);

    // Main loop
    std::cout << "Controls: [S] save PNG, [Esc] quit.\n";
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
        {
            // Save PNG
            std::string baseName = fs::path(inputPath).stem().string(); // without .bin
            std::string outName = outputDir + "/" + baseName + "_" + modeName + ".png";
            savePNG(outName, rgb, VIEW_SIZE, VIEW_SIZE);
            // wait for key release to avoid multiple saves
            while (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
                glfwWaitEvents();
        }
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(win, GL_TRUE);

        glfwSwapBuffers(win);
    }

    glfwTerminate();
    return 0;
}