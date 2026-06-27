/**
 * IconViewer – display IconsPacked.bin with full RGB range
 * =========================================================
 * Loads GameData/Icons/IconsPacked.bin (8192x8192, 9‑bit colour),
 * scales each 3‑bit channel back to 8‑bit (multiply by 32),
 * flips the image vertically to match OpenGL's texture orientation,
 * and displays the atlas full‑screen with pan & zoom.
 *
 * Compile (must use UCRT64 shell):
 *   g++ -std=c++20 -O2 -static -DGLEW_STATIC \
 *     -o IconViewer.exe IconViewer.cpp \
 *     -I/ucrt64/include -L/ucrt64/lib \
 *     -lglfw3 -lglew32 -lopengl32 -lgdi32 -luser32 -lshell32
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>

using u16 = uint16_t;
using u8 = uint8_t;

static double g_zoom = 1.0;
static double g_offsetX = 0.0, g_offsetY = 0.0;
static bool g_dragging = false;
static double g_lastMouseX, g_lastMouseY;

void scroll_cb(GLFWwindow * /*w*/, double /*x*/, double y)
{
    g_zoom *= (y > 0) ? 1.2 : 0.8;
    if (g_zoom < 0.01)
        g_zoom = 0.01;
    if (g_zoom > 50.0)
        g_zoom = 50.0;
}

void mouse_btn_cb(GLFWwindow *w, int btn, int act, int /*mods*/)
{
    if (btn == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (act == GLFW_PRESS)
        {
            g_dragging = true;
            glfwGetCursorPos(w, &g_lastMouseX, &g_lastMouseY);
        }
        else
        {
            g_dragging = false;
        }
    }
}

void cursor_cb(GLFWwindow *w, double x, double y)
{
    if (!g_dragging)
        return;
    int fbW, fbH;
    glfwGetFramebufferSize(w, &fbW, &fbH);
    double dx = (x - g_lastMouseX) / fbW / g_zoom;
    double dy = (y - g_lastMouseY) / fbH / g_zoom;
    g_offsetX -= dx;
    g_offsetY += dy;
    g_lastMouseX = x;
    g_lastMouseY = y;
}

int main()
{
    const std::string binPath = "GameData/Icons/IconsPacked.bin";
    std::ifstream file(binPath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Cannot open " << binPath << "\n";
        return 1;
    }
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    const int TEX_W = 8192, TEX_H = 8192;
    if (fileSize != TEX_W * TEX_H * sizeof(u16))
    {
        std::cerr << "Unexpected file size\n";
        return 2;
    }
    std::vector<u16> packed(TEX_W * TEX_H);
    file.read(reinterpret_cast<char *>(packed.data()), fileSize);

    // Convert 9‑bit colour to RGBA (scale 0‑7 → 0‑255 by *36)
    std::vector<u8> rgba(TEX_W * TEX_H * 4);
    for (size_t i = 0; i < (size_t)TEX_W * TEX_H; ++i)
    {
        u16 p = packed[i];
        u8 r = ((p >> 6) & 0x7) * 36;
        u8 g = ((p >> 3) & 0x7) * 36;
        u8 b = (p & 0x7) * 36;
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = 255;
    }

    // Flip the image vertically because OpenGL expects bottom‑left origin
    for (int row = 0; row < TEX_H / 2; ++row)
    {
        int topRow = row;
        int bottomRow = TEX_H - 1 - row;
        for (int col = 0; col < TEX_W * 4; ++col)
        {
            std::swap(rgba[topRow * TEX_W * 4 + col],
                      rgba[bottomRow * TEX_W * 4 + col]);
        }
    }

    if (!glfwInit())
        return 3;
    GLFWmonitor *mon = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(mon);
    GLFWwindow *win = glfwCreateWindow(mode->width, mode->height, "Icon Atlas Viewer", mon, nullptr);
    if (!win)
        return 4;
    glfwMakeContextCurrent(win);
    glfwSetScrollCallback(win, scroll_cb);
    glfwSetMouseButtonCallback(win, mouse_btn_cb);
    glfwSetCursorPosCallback(win, cursor_cb);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
        return 5;

    const char *vsSrc = R"(
    #version 330 core
    layout(location=0) in vec2 aPos;
    out vec2 vTex;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
        vTex = aPos * 0.5 + 0.5;
    }
    )";
    const char *fsSrc = R"(
    #version 330 core
    in vec2 vTex;
    out vec4 FragColor;
    uniform sampler2D uTex;
    uniform vec2 u_offset;
    uniform float u_scale;
    void main() {
        vec2 uv = (vTex - 0.5) / u_scale + u_offset;
        FragColor = texture(uTex, uv);
    }
    )";
    auto comp = [](GLenum t, const char *s)
    {
        GLuint sh = glCreateShader(t);
        glShaderSource(sh, 1, &s, nullptr);
        glCompileShader(sh);
        return sh;
    };
    GLuint prog = glCreateProgram();
    glAttachShader(prog, comp(GL_VERTEX_SHADER, vsSrc));
    glAttachShader(prog, comp(GL_FRAGMENT_SHADER, fsSrc));
    glLinkProgram(prog);
    glUseProgram(prog);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_W, TEX_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    rgba.clear();
    rgba.shrink_to_fit();

    GLfloat quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    GLint offLoc = glGetUniformLocation(prog, "u_offset");
    GLint scaleLoc = glGetUniformLocation(prog, "u_scale");

    while (!glfwWindowShouldClose(win))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glUniform2f(offLoc, (float)g_offsetX, (float)g_offsetY);
        glUniform1f(scaleLoc, (float)g_zoom);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}