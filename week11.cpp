// =====================================================================
//  11-апта: Суретті файлдан жүктеу. Текстура объектісі. Текстураланған куб.
//  (10-аптаның үстіне: түс атрибуты -> UV, stb_image, loadTexture, sampler2D)
//
//  Сурет: assets/container.png  (CMake жолды ASSET_DIR арқылы береді)
//
//  БАСҚАРУ — камера (10-аптадағыдай)
//    Тінтуір — қарау, дөңгелек — зум (fov)
//    W S A D — жүру, Space / L-Shift — жоғары / төмен, L-Ctrl — жүгіру
//    R — бастапқы күйге қайтару, P — күйді шығару, I — Invert Y, Tab — тінтуірді босату
//
//  БАСҚАРУ — 11-апта тапсырмалары
//    [ / ]   — 5-тапсырма: uvScale азайту / көбейту (0.5 ... 8.0)
//    T       — 1-тапсырма: wrap режимі  REPEAT -> MIRRORED_REPEAT -> CLAMP_TO_EDGE -> CLAMP_TO_BORDER
//    V       — 1,2-тапсырма: vertices[] ішіндегі UV көбейткіші  1.0 -> 2.0 -> 0.1 -> 1.0
//    F       — 2-тапсырма: MAG сүзгісі  LINEAR <-> NEAREST
//    M       — 3-тапсырма: MIN сүзгісі  LINEAR_MIPMAP_LINEAR <-> LINEAR (mipmap жоқ)
//    B       — 4-тапсырма: MAG-қа ӘДЕЙІ қате mipmap тұрақтысын беру (glGetError шығады)
//    J       — 4-тапсырма: mipmap жасалмаған текстура + MIN mipmap-сүзгі => шынымен қара куб
//    Y       — Қ2: tint түсі уақытпен өзгереді
//    N       — Қ3: ағып жатқан су әсері (UV уақытпен жылжиды)
//    G       — Қ4: үлкен жазық еден (анизотропты фильтрацияға қиғаш қара)
//    K       — Қ4: анизотропты фильтрация қосу / өшіру
//    ESC     — шығу
//  Режимдер терезе тақырыбында және консольде көрінеді.
// =====================================================================
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION      // 1-қадам: #include-тан БҰРЫН. Онсыз "undefined reference to stbi_load"
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifndef ASSET_DIR                      // 2-қадам: CMake береді; бермесе — ағымдағы бумадағы assets/
#define ASSET_DIR "assets"
#endif

int gWidth = 800, gHeight = 600;

// ---------- камера (9-10 апта) ----------
const glm::vec3 CAM_START(0.0f, 0.0f, 3.0f);
glm::vec3 camPos   = CAM_START;
glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camUp    = glm::vec3(0.0f, 1.0f, 0.0f);
const float YAW_START = -90.0f, FOV_START = 45.0f;
float yaw = YAW_START, pitch = 0.0f, lastX = 400.0f, lastY = 300.0f, fov = FOV_START;
bool  firstMouse = true, invertY = false, mouseOn = true;
const float sensitivity = 0.1f;

// ---------- 11-апта: текстура күйі ----------
float uvScale = 1.0f;                   // 5-тапсырма
const GLint   WRAP_MODES[4] = { GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER };
const char*   WRAP_NAMES[4] = { "REPEAT", "MIRRORED_REPEAT", "CLAMP_TO_EDGE", "CLAMP_TO_BORDER" };
int   wrapIdx = 0;
const float UV_MULTS[3] = { 1.0f, 2.0f, 0.1f };
int   uvMultIdx = 0;
bool  magNearest = false, minMipmap = true, brokenMag = false, noMipTexture = false;
bool  tintAnim = false, flow = false, showFloor = false, aniso = false;

void onResize(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
    gWidth = w;
    gHeight = (h > 0) ? h : 1;
}

void updateFront() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(dir);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (!mouseOn) return;
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    if (invertY) yoffset = -yoffset;
    lastX = (float)xpos;
    lastY = (float)ypos;
    yaw   += xoffset * sensitivity;
    pitch += yoffset * sensitivity;
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    if (yaw >= 360.0f) yaw -= 360.0f;
    if (yaw <= -360.0f) yaw += 360.0f;
    updateFront();
}

void scroll_callback(GLFWwindow*, double, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 90.0f) fov = 90.0f;
}

bool pressedOnce(GLFWwindow* window, int key) {
    static bool prev[GLFW_KEY_LAST + 1] = {};
    bool now = glfwGetKey(window, key) == GLFW_PRESS;
    bool fired = now && !prev[key];
    prev[key] = now;
    return fired;
}

// ---------- 6-қадам: текстураны жүктеу ----------
// genMipmap = false — тек 4-тапсырманың «шынайы қара куб» тәжірибесі үшін
unsigned int loadTexture(const char* path, bool genMipmap = true) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);                        // «ұя»: төмендегі бәрі осы объектіге барады

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);   // MAG-қа mipmap тұрақтысы ЖАЗЫЛМАЙДЫ

    stbi_set_flip_vertically_on_load(true);                  // stbi_load-тан БҰРЫН: сурет төңкерілмейді

    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if (data) {
        GLenum fmt = (n == 4) ? GL_RGBA : GL_RGB;            // арна санына қарай
        if (n == 1) fmt = GL_RED;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);               // RGB жолдары 4-ке еселі болмаса да дұрыс оқылады
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        if (genMipmap) glGenerateMipmap(GL_TEXTURE_2D);      // glTexImage2D-ден КЕЙІН
        std::cout << path << " " << w << "x" << h << " арна: " << n << std::endl;
    } else {
        std::cerr << "Сурет жүктелмеді: " << path << std::endl;
    }
    stbi_image_free(data);                                   // дерек GPU-ға көшті, CPU көшірмесі керек емес
    return id;
}

// Текстура параметрлерін ағымдағы режимге сай қайта қою (T, F, M, B, K пернелері)
void applyTexParams(unsigned int tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, WRAP_MODES[wrapIdx]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, WRAP_MODES[wrapIdx]);
    const float border[] = { 1.0f, 0.0f, 0.0f, 1.0f };       // CLAMP_TO_BORDER үшін қызыл шекара
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magNearest ? GL_NEAREST : GL_LINEAR);

    if (brokenMag) {                                         // 4-тапсырма: әдейі қате
        while (glGetError() != GL_NO_ERROR) {}
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        GLenum err = glGetError();
        std::cout << "MAG = LINEAR_MIPMAP_LINEAR -> glGetError() = "
                  << (err == GL_INVALID_ENUM ? "GL_INVALID_ENUM" : std::to_string(err)) << std::endl;
    }

    if (aniso) {                                             // Қ4: анизотропты фильтрация
        while (glGetError() != GL_NO_ERROR) {}
        float maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        if (glGetError() == GL_NO_ERROR && maxAniso > 1.0f) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);
            std::cout << "anisotropy = " << maxAniso << std::endl;
        } else {
            std::cout << "анизотропты фильтрация қолдау таппады" << std::endl;
        }
    } else {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
        glGetError();
    }
}

// ---------- 5-қадам: шейдерлер ----------
const char* vsSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
}
)";
const char* fsSrc = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;   // сілтеме: текстура бірлігінің нөмірі
uniform float uvScale;          // 5-тапсырма
uniform float uvTile;           // еденге: қосымша қайталау
uniform float time;             // Қ3
uniform vec3  tint;             // Қ2
void main() {
    vec2 uv = vTexCoord * uvScale * uvTile + vec2(0.0, time * 0.1);
    FragColor = texture(ourTexture, uv) * vec4(tint, 1.0);   // texture(), texture2D емес!
}
)";

unsigned int makeShader() {
    auto compile = [](GLenum type, const char* src) {
        unsigned int id = glCreateShader(type);
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);
        int ok;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) { char log[512]; glGetShaderInfoLog(id, 512, nullptr, log); std::cerr << log << "\n"; }
        return id;
    };
    unsigned int vs = compile(GL_VERTEX_SHADER, vsSrc), fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------- 3-қадам: vertices[] = позиция(3) + UV(2), 36 вершина ----------
// Позициялар 10-аптадағыдай CCW. Әр беттің UV реті бірдей:
//   (0,0) (1,0) (1,1)   бірінші үшбұрыш
//   (0,0) (1,1) (0,1)   екінші үшбұрыш
std::vector<float> buildVertices() {
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {
        {{ 1, 0, 0}, {0, 0,-1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{ 0, 1, 0}, {1, 0, 0}, {0, 0,-1}}, {{ 0,-1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{ 0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{ 0, 0,-1}, {-1,0, 0}, {0, 1, 0}},
    };
    const float cu[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, cv[4] = { -0.5f, -0.5f, 0.5f, 0.5f };
    const float uvu[4] = { 0, 1, 1, 0 }, uvv[4] = { 0, 0, 1, 1 };
    const int order[6] = { 0, 1, 2, 0, 2, 3 };
    std::vector<float> out;
    for (const Face& f : faces)
        for (int k : order) {
            glm::vec3 p = f.n * 0.5f + f.u * cu[k] + f.v * cv[k];
            out.insert(out.end(), { p.x, p.y, p.z, uvu[k], uvv[k] });
        }
    return out;                                              // 6 бет x 6 вершина x 5 сан = 180
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW iske qosylmady\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    GLFWwindow* window = glfwCreateWindow(gWidth, gHeight, "Week 11 - textures", nullptr, nullptr);
    if (!window) { std::cerr << "Tereze zhasalmady\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, onResize);
    glfwSwapInterval(1);
    if (gladLoadGL(glfwGetProcAddress) == 0) { std::cerr << "GLAD zhuktelmedi\n"; return -1; }
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    updateFront();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // ---------- VAO / VBO ----------
    std::vector<float> baseVerts = buildVertices();
    std::vector<float> verts = baseVerts;
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    // 4-қадам: stride 5 (ЕКІ жерде де), UV — 2 компонент
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    unsigned int shader = makeShader();
    std::string path = std::string(ASSET_DIR) + "/container.png";   // қысқа "container.png" емес!
    std::cout << "Сурет: " << path << std::endl;
    unsigned int texture = loadTexture(path.c_str());
    unsigned int textureNoMip = loadTexture(path.c_str(), false);   // тек 4-тапсырмаға (J)
    glBindTexture(GL_TEXTURE_2D, textureNoMip);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);   // mipmap жоқ, бірақ сүзгі соны күтеді

    int locModel = glGetUniformLocation(shader, "model"), locView = glGetUniformLocation(shader, "view");
    int locProj = glGetUniformLocation(shader, "projection"), locTex = glGetUniformLocation(shader, "ourTexture");
    int locUV = glGetUniformLocation(shader, "uvScale"), locTile = glGetUniformLocation(shader, "uvTile");
    int locTime = glGetUniformLocation(shader, "time"), locTint = glGetUniformLocation(shader, "tint");

    // 8-аптадағы бес куб + 3-тапсырма: z = -40 маңындағы 6 куб
    std::vector<glm::vec3> cubePositions = {
        {0.0f, 0.0f, 0.0f}, {2.0f, 0.5f, -2.0f}, {-1.8f, -0.3f, -1.0f}, {1.2f, -0.8f, -3.5f}, {-2.2f, 1.0f, -4.0f},
        {0.0f, 0.0f, -40.0f}, {3.0f, 1.0f, -42.0f}, {-3.0f, -1.0f, -38.0f}, {6.0f, 0.0f, -40.0f}, {-6.0f, 1.0f, -41.0f}, {0.0f, 3.0f, -44.0f}
    };

    std::string lastTitle;
    float last = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime(), dt = std::min(now - last, 0.05f);
        last = now;

        // ---------- енгізу ----------
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        float speed = 2.5f * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) speed *= 2.0f;
        glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += speed * camFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= speed * camFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= right * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += right * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camPos += speed * camUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camPos -= speed * camUp;
        if (glm::length(camPos) > 20.0f) camPos = glm::normalize(camPos) * 20.0f;

        // 5-тапсырма: [ және ] — үздіксіз әрекет, сондықтан dt-ге көбейтіледі
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) uvScale += 2.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)  uvScale -= 2.0f * dt;
        uvScale = glm::clamp(uvScale, 0.5f, 8.0f);

        bool texChanged = false;
        if (pressedOnce(window, GLFW_KEY_T)) { wrapIdx = (wrapIdx + 1) % 4; texChanged = true; }
        if (pressedOnce(window, GLFW_KEY_F)) { magNearest = !magNearest; texChanged = true; }
        if (pressedOnce(window, GLFW_KEY_M)) { minMipmap = !minMipmap; texChanged = true; }
        if (pressedOnce(window, GLFW_KEY_B)) { brokenMag = !brokenMag; texChanged = true; }
        if (pressedOnce(window, GLFW_KEY_K)) { aniso = !aniso; texChanged = true; }
        if (pressedOnce(window, GLFW_KEY_J)) noMipTexture = !noMipTexture;
        if (pressedOnce(window, GLFW_KEY_Y)) tintAnim = !tintAnim;
        if (pressedOnce(window, GLFW_KEY_N)) flow = !flow;
        if (pressedOnce(window, GLFW_KEY_G)) showFloor = !showFloor;
        if (texChanged) applyTexParams(texture);

        if (pressedOnce(window, GLFW_KEY_V)) {                       // vertices[] ішіндегі UV-ді көбейту
            uvMultIdx = (uvMultIdx + 1) % 3;
            for (size_t i = 0; i < verts.size(); i += 5) {
                verts[i + 3] = baseVerts[i + 3] * UV_MULTS[uvMultIdx];
                verts[i + 4] = baseVerts[i + 4] * UV_MULTS[uvMultIdx];
            }
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
        }
        if (pressedOnce(window, GLFW_KEY_I)) invertY = !invertY;
        if (pressedOnce(window, GLFW_KEY_R)) {
            camPos = CAM_START; yaw = YAW_START; pitch = 0.0f; fov = FOV_START; updateFront();
        }
        if (pressedOnce(window, GLFW_KEY_P)) {
            std::cout << std::fixed << std::setprecision(2)
                      << "camPos = (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")  yaw = " << yaw
                      << "  pitch = " << pitch << "  fov = " << fov << "  uvScale = " << uvScale << std::endl;
        }
        if (pressedOnce(window, GLFW_KEY_TAB)) {
            mouseOn = !mouseOn;
            glfwSetInputMode(window, GLFW_CURSOR, mouseOn ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (mouseOn) firstMouse = true;
        }

        // ---------- сызу ----------
        glClearColor(0.07f, 0.08f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)gWidth / (float)gHeight, 0.1f, 100.0f);
        glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));

        // 7-қадам: текстураны байлау (бір текстураға міндетті емес, бірақ 12-аптаға әдет)
        glUniform1i(locTex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, noMipTexture ? textureNoMip : texture);

        glUniform1f(locUV, uvScale);
        glUniform1f(locTile, 1.0f);
        glUniform1f(locTime, flow ? now : 0.0f);
        if (tintAnim) glUniform3f(locTint, 0.6f + 0.4f * std::sin(now), 0.6f + 0.4f * std::sin(now + 2.1f), 0.6f + 0.4f * std::sin(now + 4.2f));
        else          glUniform3f(locTint, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(VAO);
        for (size_t i = 0; i < cubePositions.size(); i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            model = glm::rotate(model, now * (0.5f + 0.3f * (i % 5)), glm::vec3(0.3f, 1.0f, 0.0f));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        if (showFloor) {                                             // Қ4: үлкен жазық «еден»
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.0f, -5.0f));
            model = glm::scale(model, glm::vec3(40.0f, 0.2f, 40.0f));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(locTile, 20.0f);                             // еденде сурет 20x20 рет қайталанады
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "wrap:%s  uvMul:%.1f  uvScale:%.2f  MAG:%s  MIN:%s%s%s  fov:%.0f",
                 WRAP_NAMES[wrapIdx], UV_MULTS[uvMultIdx], uvScale, magNearest ? "NEAREST" : "LINEAR",
                 minMipmap ? "MIPMAP" : "LINEAR", brokenMag ? "  [BROKEN MAG]" : "",
                 noMipTexture ? "  [NO MIPMAP TEX]" : "", fov);
        if (lastTitle != buf) { glfwSetWindowTitle(window, buf); lastTitle = buf; }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}