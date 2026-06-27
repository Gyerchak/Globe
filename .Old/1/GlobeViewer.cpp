#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>

using u64 = uint64_t;

// ============================================================
//  STATE
// ============================================================
static float g_quat[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // globe rotation
static float g_distance = 3.5f;
static const float MIN_DIST = 1.01886f;
static const float MAX_DIST = 10.0f;

// Camera pan offset (world units)
static float g_panX = 0.0f, g_panY = 0.0f;

// Initial values
static float g_initialQuat[4];
static const float START_YAW = 30.0f;
static const float START_PITCH = 45.0f;
static const float START_ROLL = 5.0f;
static const float START_DIST = 3.0f;

// Mouse state
static bool g_rightMouseDown = false;
static double g_lastMouseX = 0.0, g_lastMouseY = 0.0;
static bool g_middleMouseDown = false;
static double g_lastMidMouseX = 0.0, g_lastMidMouseY = 0.0;

static int g_fbWidth, g_fbHeight;
static bool g_showWater = true;

// ============================================================
//  QUATERNION HELPERS
// ============================================================
static void quat_normalize(float q[4])
{
    float len = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len < 1e-6f)
    {
        q[0] = 1;
        q[1] = q[2] = q[3] = 0;
        return;
    }
    q[0] /= len;
    q[1] /= len;
    q[2] /= len;
    q[3] /= len;
}
static void quat_multiply(float out[4], const float a[4], const float b[4])
{
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    out[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}
static void quat_to_mat4(const float q[4], float m[16])
{
    float w = q[0], x = q[1], y = q[2], z = q[3];
    m[0] = 1 - 2 * y * y - 2 * z * z;
    m[4] = 2 * x * y - 2 * w * z;
    m[8] = 2 * x * z + 2 * w * y;
    m[12] = 0;
    m[1] = 2 * x * y + 2 * w * z;
    m[5] = 1 - 2 * x * x - 2 * z * z;
    m[9] = 2 * y * z - 2 * w * x;
    m[13] = 0;
    m[2] = 2 * x * z - 2 * w * y;
    m[6] = 2 * y * z + 2 * w * x;
    m[10] = 1 - 2 * x * x - 2 * y * y;
    m[14] = 0;
    m[3] = 0;
    m[7] = 0;
    m[11] = 0;
    m[15] = 1;
}
static void eulerToQuat(float yawDeg, float pitchDeg, float rollDeg, float q[4])
{
    float yaw = yawDeg * 3.14159f / 180.0f, pitch = pitchDeg * 3.14159f / 180.0f, roll = rollDeg * 3.14159f / 180.0f;
    float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f);
    float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    float cr = cosf(roll * 0.5f), sr = sinf(roll * 0.5f);
    float qX[4] = {cp, sp, 0, 0}, qY[4] = {cy, 0, sy, 0}, qZ[4] = {cr, 0, 0, sr};
    float tmp[4];
    quat_multiply(tmp, qX, qZ);
    quat_multiply(q, qY, tmp);
}

// ============================================================
//  CALLBACKS
// ============================================================
void scroll_callback(GLFWwindow * /*w*/, double /*x*/, double yoffset)
{
    float factor = (yoffset > 0.0) ? 0.97f : 1.0f / 0.97f;
    g_distance *= factor;
    if (g_distance < MIN_DIST)
        g_distance = MIN_DIST;
    if (g_distance > MAX_DIST)
        g_distance = MAX_DIST;
}

void mouse_button_callback(GLFWwindow *w, int btn, int act, int /*mods*/)
{
    if (btn == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (act == GLFW_PRESS)
        {
            g_rightMouseDown = true;
            glfwGetCursorPos(w, &g_lastMouseX, &g_lastMouseY);
        }
        else
            g_rightMouseDown = false;
    }
    else if (btn == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (act == GLFW_PRESS)
        {
            g_middleMouseDown = true;
            glfwGetCursorPos(w, &g_lastMidMouseX, &g_lastMidMouseY);
        }
        else
            g_middleMouseDown = false;
    }
}

void cursor_pos_callback(GLFWwindow *window, double x, double y)
{
    // Middle‑drag (pan)
    if (g_middleMouseDown)
    {
        double dx = x - g_lastMidMouseX;
        double dy = y - g_lastMidMouseY;
        g_lastMidMouseX = x;
        g_lastMidMouseY = y;
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float panScale = (g_distance * 0.5f) / fbH;
        g_panX += (float)dx * panScale;
        g_panY -= (float)dy * panScale;
        return;
    }
    // Right‑drag (rotate globe)
    if (!g_rightMouseDown)
        return;

    double dx = x - g_lastMouseX;
    double dy = y - g_lastMouseY;
    g_lastMouseX = x;
    g_lastMouseY = y;

    bool shift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                  glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    if (shift)
    {
        float rollAngle = (float)dx * 0.0015f;
        float q[4] = {cosf(rollAngle / 2), 0, 0, sinf(rollAngle / 2)};
        float newQ[4];
        quat_multiply(newQ, q, g_quat);
        memcpy(g_quat, newQ, sizeof(newQ));
        quat_normalize(g_quat);
    }
    else
    {
        const float sensitivity = 0.05f;
        float yawDelta = -(float)dx * sensitivity;
        float pitchDelta = (float)dy * sensitivity;
        float ang = yawDelta * 3.14159f / 180.0f;
        float stepQuat[4] = {cosf(ang / 2), 0, sinf(ang / 2), 0};
        float newQ[4];
        quat_multiply(newQ, stepQuat, g_quat);
        memcpy(g_quat, newQ, sizeof(newQ));
        ang = -pitchDelta * 3.14159f / 180.0f;
        stepQuat[0] = cosf(ang / 2);
        stepQuat[1] = sinf(ang / 2);
        stepQuat[2] = 0;
        stepQuat[3] = 0;
        quat_multiply(newQ, stepQuat, g_quat);
        memcpy(g_quat, newQ, sizeof(newQ));
        quat_normalize(g_quat);
    }
}

void key_callback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_V)
            g_showWater = !g_showWater;
    }
}

// ============================================================
//  LOAD TEXTURE (RGBA: height + water mask)
// ============================================================
static std::vector<uint8_t> loadHeightWithWater(const std::string &path, int &w, int &h)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "CANNOT OPEN: " << path << "\n";
        return {};
    }
    const int S = 8192;
    std::vector<u64> packed(S * S);
    file.read((char *)packed.data(), S * S * sizeof(u64));
    if (!file)
    {
        std::cerr << "File too small\n";
        return {};
    }
    w = h = S;
    std::vector<uint8_t> rgba(S * S * 4);
    for (int i = 0; i < S * S; ++i)
    {
        u64 v = packed[i];
        uint8_t height = (v >> 28) & 0xFF;
        uint8_t water = ((v >> 36) & 0x3) ? 255 : 0;
        rgba[i * 4 + 0] = height;
        rgba[i * 4 + 1] = height;
        rgba[i * 4 + 2] = height;
        rgba[i * 4 + 3] = water;
    }
    return rgba;
}

// ============================================================
//  SPHERE MESH
// ============================================================
struct SphereMesh
{
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    int indexCount;
};
static SphereMesh createSphere(int latDiv, int lonDiv, float radius)
{
    SphereMesh mesh;
    for (int i = 0; i <= latDiv; ++i)
    {
        float theta = i * 3.14159265358979f / latDiv;
        float sinT = sinf(theta), cosT = cosf(theta);
        for (int j = 0; j <= lonDiv; ++j)
        {
            float phi = j * 2.0f * 3.14159265358979f / lonDiv;
            float sinP = sinf(phi), cosP = cosf(phi);
            float x = cosP * sinT, y = cosT, z = sinP * sinT;
            mesh.vertices.push_back(radius * x);
            mesh.vertices.push_back(radius * y);
            mesh.vertices.push_back(radius * z);
            mesh.vertices.push_back(x);
            mesh.vertices.push_back(y);
            mesh.vertices.push_back(z);
            float u = 1.0f - (float)j / lonDiv;
            float v = (float)i / latDiv;
            mesh.vertices.push_back(u);
            mesh.vertices.push_back(v);
        }
    }
    for (int i = 0; i < latDiv; ++i)
        for (int j = 0; j < lonDiv; ++j)
        {
            int first = i * (lonDiv + 1) + j, second = first + lonDiv + 1;
            mesh.indices.push_back(first);
            mesh.indices.push_back(second);
            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(second);
            mesh.indices.push_back(second + 1);
            mesh.indices.push_back(first + 1);
        }
    mesh.indexCount = (int)mesh.indices.size();
    return mesh;
}

// ============================================================
//  SHADERS
// ============================================================
const char *vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;
out vec2 vTex;
uniform mat4 model, view, proj;
uniform sampler2D heightMap;
uniform float u_displacementScale;

void main() {
    float h = texture(heightMap, aTex).r;
    float displacement = h * u_displacementScale;
    vec3 newPos = aPos + aNormal * displacement;
    gl_Position = proj * view * model * vec4(newPos, 1.0);
    vTex = aTex;
}
)";
const char *fsSrc = R"(
#version 330 core
in vec2 vTex;
out vec4 FragColor;
uniform sampler2D heightMap;
uniform bool u_showWater;

void main() {
    vec4 tex = texture(heightMap, vTex);
    vec3 terrain = tex.rgb;
    float mask   = tex.a;
    vec3 waterColor = vec3(0.0, 0.3, 1.0);
    if (u_showWater) {
        vec3 color = mix(waterColor, terrain, mask);
        FragColor = vec4(color, 1.0);
    } else {
        FragColor = vec4(terrain, 1.0);
    }
}
)";

// ============================================================
//  MAIN
// ============================================================
int main()
{
    eulerToQuat(START_YAW, START_PITCH, START_ROLL, g_initialQuat);
    memcpy(g_quat, g_initialQuat, sizeof(g_quat));
    g_distance = START_DIST;
    g_panX = g_panY = 0.0f;

    int texW, texH;
    auto rgba = loadHeightWithWater("GameData/Map/BaseStaticMap.bin", texW, texH);
    if (rgba.empty())
    {
        std::cout << "Press Enter\n";
        std::cin.get();
        return 1;
    }

    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWmonitor *mon = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(mon);
    GLFWwindow *win = glfwCreateWindow(mode->width, mode->height, "Globe Viewer", mon, nullptr);
    if (!win)
        return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetScrollCallback(win, scroll_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetCursorPosCallback(win, cursor_pos_callback);
    glfwSetKeyCallback(win, key_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
        return 1;
    glEnable(GL_DEPTH_TEST);

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    SphereMesh sphere = createSphere(128, 256, 1.0f);
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sphere.vertices.size() * sizeof(float), sphere.vertices.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphere.indices.size() * sizeof(uint32_t), sphere.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    GLint modelLoc = glGetUniformLocation(prog, "model");
    GLint viewLoc = glGetUniformLocation(prog, "view");
    GLint projLoc = glGetUniformLocation(prog, "proj");
    GLint dispLoc = glGetUniformLocation(prog, "u_displacementScale");
    GLint showWaterLoc = glGetUniformLocation(prog, "u_showWater");
    glUniform1f(dispLoc, 0.02f);

    const float BASE_ROT_SPEED = 30.0f; // degrees per second (normal)
    const float MIN_ROT_SPEED = 7.0f;
    const float ZOOM_FACTOR = 1.2f; // keyboard zoom speed multiplier
    const float PAN_SPEED = 0.5f;   // base pan speed (world units per second)
    const float FAST_MULT = 2.0f;   // multiplier for shift‑held fast mode

    double lastTime = glfwGetTime();
    bool wasReset1 = false;

    while (!glfwWindowShouldClose(win))
    {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.1f)
            dt = 0.1f;
        if (dt <= 0.0f)
            dt = 0.001f;

        // ---- Shift detection (used for fast mode) ----
        bool shiftHeld = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                          glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        float fastFactor = shiftHeld ? FAST_MULT : 1.0f;

        // ---- Poll keys ----
        bool w = (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS);
        bool s = (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS);
        bool a = (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS);
        bool d = (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS);
        bool q = (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS);
        bool e = (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS);

        bool up = (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS);
        bool down = (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS);
        bool left = (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS);
        bool right = (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS);

        bool zoomIn = (glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS);
        bool zoomOut = (glfwGetKey(win, GLFW_KEY_X) == GLFW_PRESS);
        bool resetKey = (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS);

        // ---- Pan (arrows) – 4x slower ----
        float panSpeed = PAN_SPEED * g_distance * dt * 0.25f; // multiplied by 0.25 = 1/4
        if (up)
            g_panY += panSpeed;
        if (down)
            g_panY -= panSpeed;
        if (left)
            g_panX -= panSpeed;
        if (right)
            g_panX += panSpeed;

        // ---- Globe rotation (WASD) – fast mode with Shift ----
        float t = (g_distance - MIN_DIST) / (MAX_DIST - MIN_DIST);
        float curRotSpeed = (MIN_ROT_SPEED + t * (BASE_ROT_SPEED - MIN_ROT_SPEED)) * fastFactor;
        float rotAngle = curRotSpeed * dt * 3.14159f / 180.0f;
        float stepQuat[4], newQ[4];

        if (w)
        {
            float ang = rotAngle;
            stepQuat[0] = cosf(ang / 2);
            stepQuat[1] = sinf(ang / 2);
            stepQuat[2] = 0;
            stepQuat[3] = 0;
            quat_multiply(newQ, stepQuat, g_quat);
            memcpy(g_quat, newQ, sizeof(newQ));
        }
        if (s)
        {
            float ang = -rotAngle;
            stepQuat[0] = cosf(ang / 2);
            stepQuat[1] = sinf(ang / 2);
            stepQuat[2] = 0;
            stepQuat[3] = 0;
            quat_multiply(newQ, stepQuat, g_quat);
            memcpy(g_quat, newQ, sizeof(newQ));
        }
        if (a)
        {
            float ang = rotAngle;
            stepQuat[0] = cosf(ang / 2);
            stepQuat[1] = 0;
            stepQuat[2] = sinf(ang / 2);
            stepQuat[3] = 0;
            quat_multiply(newQ, stepQuat, g_quat);
            memcpy(g_quat, newQ, sizeof(newQ));
        }
        if (d)
        {
            float ang = -rotAngle;
            stepQuat[0] = cosf(ang / 2);
            stepQuat[1] = 0;
            stepQuat[2] = sinf(ang / 2);
            stepQuat[3] = 0;
            quat_multiply(newQ, stepQuat, g_quat);
            memcpy(g_quat, newQ, sizeof(newQ));
        }

        // Roll (Q/E) – fast mode with Shift
        float rollAngle = 30.0f * dt * 3.14159f / 180.0f * fastFactor;
        if (q)
        {
            float ang = -rollAngle;
            stepQuat[0] = cosf(ang / 2);
            stepQuat[1] = 0;
            stepQuat[2] = 0;
            stepQuat[3] = sinf(ang / 2);
            quat_multiply(newQ, stepQuat, g_quat);
            memcpy(g_quat, newQ, sizeof(newQ));
        }
        if (e)
        {
            float ang = rollAngle;
            stepQuat[0] = cosf(ang / 2);
            stepQuat[1] = 0;
            stepQuat[2] = 0;
            stepQuat[3] = sinf(ang / 2);
            quat_multiply(newQ, stepQuat, g_quat);
            memcpy(g_quat, newQ, sizeof(newQ));
        }

        quat_normalize(g_quat);

        // ---- Zoom (Z / X) – fast mode with Shift ----
        float curZoomFactor = ZOOM_FACTOR * fastFactor;
        if (zoomIn)
            g_distance *= powf(1.0f / curZoomFactor, dt);
        if (zoomOut)
            g_distance *= powf(curZoomFactor, dt);
        if (g_distance < MIN_DIST)
            g_distance = MIN_DIST;
        if (g_distance > MAX_DIST)
            g_distance = MAX_DIST;

        // ---- Reset ----
        if (resetKey && !wasReset1)
        {
            memcpy(g_quat, g_initialQuat, sizeof(g_quat));
            g_distance = START_DIST;
            g_panX = g_panY = 0.0f;
        }
        wasReset1 = resetKey;

        // ---- Render ----
        glfwGetFramebufferSize(win, &g_fbWidth, &g_fbHeight);
        glViewport(0, 0, g_fbWidth, g_fbHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (float)g_fbWidth / g_fbHeight;
        float proj[16] = {0};
        float fov = 45.0f * 3.14159f / 180.0f;
        proj[0] = 1.0f / (aspect * tanf(fov / 2));
        proj[5] = 1.0f / tanf(fov / 2);
        float near = 0.00001f, far = 100.0f;
        proj[10] = -(far + near) / (far - near);
        proj[11] = -1.0f;
        proj[14] = -(2.0f * far * near) / (far - near);

        float view[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            g_panX, g_panY, -g_distance, 1};
        float model[16];
        quat_to_mat4(g_quat, model);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj);
        glUniform1i(showWaterLoc, g_showWater ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}