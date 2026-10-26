#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <GL/gl.h>
#include <GL/wglext.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "Camera.h"
#include "GLFunctions.h"
#include "Math.h"
#include "ModelTransform.h"
#include "ObjModel.h"
#include "Renderer.h"
#include "TextRenderer.h"

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;
constexpr const char* kWindowClass = "OpenGLAssignmentViewer";

std::string exeDirectory() {
    char buffer[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::string path(buffer);
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

bool fileExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::string wideToAnsi(const wchar_t* text) {
    if (!text) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text, -1, result.data(), size, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}

float interfaceScale(int width, int height) {
    return og::clampf(std::min(static_cast<float>(width) / 1280.0f,
                               static_cast<float>(height) / 800.0f), 0.85f, 1.35f);
}

} // namespace

class Viewer {
public:
    bool initialize(HINSTANCE instance, int showCommand);
    void run();
    void shutdown();

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    bool createWindow(HINSTANCE instance, int showCommand);
    bool createOpenGL();
    bool loadAsset(const std::vector<std::string>& arguments);
    bool loadModelFile(const std::string& path);
    bool openObjDialog();
    void update(float deltaSeconds);
    void render();
    void drawHud();
    void drawAxisGizmo(float logicalWidth);
    void drawInputMonitor(float logicalWidth, float logicalHeight);
    bool hitAxisGizmo(int x, int y, int& axis, int& sign) const;
    void setHudFontScale(float scale);
    void writeLog(const std::string& message);
    void showFatalError(const std::string& message);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HDC deviceContext_ = nullptr;
    HGLRC renderContext_ = nullptr;
    int width_ = kInitialWidth;
    int height_ = kInitialHeight;
    bool running_ = false;
    bool showGrid_ = true;
    bool showWireframe_ = false;
    bool showHud_ = true;
    bool showInputMonitor_ = true;
    float wheelFlash_ = 0.0f;

    bool keys_[256] = {};
    bool leftDragging_ = false;
    bool rightDragging_ = false;
    POINT lastMouse_{};

    ObjModel model_;
    Camera camera_;
    ModelTransform transform_;
    Renderer renderer_;
    TextRenderer ui_;
    std::string logPath_;
    std::string assetName_;
    double lastTime_ = 0.0;
    float fps_ = 60.0f;
    float fpsTimer_ = 0.0f;
    bool rendererReady_ = false;
    int viewAxis_ = -1;
    int viewSign_ = 1;
    float hudFontScale_ = 1.0f;
    bool axisGizmoVisible_ = false;
    float axisGizmoCenterX_ = 0.0f;
    float axisGizmoCenterY_ = 0.0f;
    float axisGizmoRadius_ = 0.0f;
    float axisHitRadius_ = 0.0f;
    float axisHitX_[3] = {};
    float axisHitY_[3] = {};
    float negativeHitX_[3] = {};
    float negativeHitY_[3] = {};
    float negativeHitHalfWidth_ = 0.0f;
    float negativeHitHalfHeight_ = 0.0f;
};

bool Viewer::initialize(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    logPath_ = exeDirectory() + "/viewer.log";
    SetProcessDPIAware();

    int argumentCount = 0;
    wchar_t** wideArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    std::vector<std::string> arguments;
    for (int i = 0; i < argumentCount; ++i) {
        arguments.push_back(wideToAnsi(wideArguments[i]));
    }
    if (wideArguments) {
        LocalFree(wideArguments);
    }

    if (!loadAsset(arguments)) {
        return false;
    }
    camera_.setFramingRadius(model_.boundingRadius());
    if (!createWindow(instance, showCommand)) {
        return false;
    }
    if (!createOpenGL()) {
        return false;
    }

    std::string error;
    if (!renderer_.init(error)) {
        showFatalError(error);
        return false;
    }
    rendererReady_ = true;
    renderer_.buildModelBuffers(model_);

    if (!ui_.init(interfaceScale(width_, height_), error)) {
        showFatalError(error);
        return false;
    }

    ShowWindow(window_, showCommand);
    ShowWindow(window_, SW_SHOWNORMAL);
    UpdateWindow(window_);
    SetForegroundWindow(window_);
    SetFocus(window_);

    LARGE_INTEGER frequency{};
    LARGE_INTEGER counter{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    lastTime_ = static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
    fpsTimer_ = 0.0f;
    running_ = true;

    char logBuffer[512];
    std::snprintf(logBuffer, sizeof(logBuffer),
                  "Renderer: %s | Vendor: %s | Model: %s | Triangles: %u | Materials: %zu",
                  reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                  reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
                  model_.name().c_str(), model_.triangleCount(), model_.materials().size());
    writeLog(logBuffer);
    return true;
}

bool Viewer::loadAsset(const std::vector<std::string>& arguments) {
    std::vector<std::string> candidates;
    for (std::size_t i = 1; i < arguments.size(); ++i) {
        if (!arguments[i].empty() && arguments[i][0] != '-' && fileExists(arguments[i])) {
            candidates.push_back(arguments[i]);
        }
    }
    const std::string executableDirectory = exeDirectory();
    candidates.push_back("assets/models/rm2019.obj");
    candidates.push_back(executableDirectory + "/../assets/models/rm2019.obj");
    candidates.push_back(executableDirectory + "/assets/models/rm2019.obj");
    candidates.push_back("assets/models/drone.obj");
    candidates.push_back(executableDirectory + "/../assets/models/drone.obj");
    candidates.push_back(executableDirectory + "/assets/models/drone.obj");

    for (const std::string& candidate : candidates) {
        if (fileExists(candidate)) {
            return loadModelFile(candidate);
        }
    }

    showFatalError("No OBJ file found. Pass a model path on the command line, drop an OBJ "
                   "onto the window, or keep assets/models/drone.obj next to the project.");
    return false;
}

bool Viewer::loadModelFile(const std::string& path) {
    std::string error;
    ObjModel loaded;
    if (!loaded.load(path, error)) {
        showFatalError(error);
        return false;
    }

    model_ = std::move(loaded);
    assetName_ = model_.name();
    camera_.setFramingRadius(model_.boundingRadius());
    camera_.reset();
    viewAxis_ = -1;
    transform_.reset();
    if (rendererReady_) {
        renderer_.buildModelBuffers(model_);
    }
    if (window_) {
        const std::string title = "OpenGL OBJ Viewer | " + assetName_;
        SetWindowTextA(window_, title.c_str());
    }
    writeLog("Loaded " + path);
    return true;
}

bool Viewer::openObjDialog() {
    char file[MAX_PATH] = {};
    std::string filter = "Wavefront OBJ (*.obj)";
    filter.push_back(0);
    filter += "*.obj";
    filter.push_back(0);
    filter += "All files (*.*)";
    filter.push_back(0);
    filter += "*.*";
    filter.push_back(0);
    filter.push_back(0);

    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = file;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = "Open OBJ model";
    dialog.lpstrDefExt = "obj";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&dialog)) {
        return false;
    }
    return loadModelFile(file);
}

bool Viewer::createWindow(HINSTANCE instance, int showCommand) {
    (void)showCommand;
    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXA);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = &Viewer::windowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassExA(&windowClass)) {
        showFatalError("Could not register the window class.");
        return false;
    }

    RECT workArea{};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
    const int availableWidth = std::max(720, static_cast<int>(workArea.right - workArea.left) - 80);
    const int availableHeight = std::max(520, static_cast<int>(workArea.bottom - workArea.top) - 100);
    const int clientWidth = std::min(kInitialWidth, availableWidth);
    const int clientHeight = std::min(kInitialHeight, availableHeight);

    RECT windowRect{0, 0, clientWidth, clientHeight};
    AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const int windowWidth = windowRect.right - windowRect.left;
    const int windowHeight = windowRect.bottom - windowRect.top;
    const int x = workArea.left + ((workArea.right - workArea.left) - windowWidth) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - windowHeight) / 2;

    window_ = CreateWindowExA(0, kWindowClass, "OpenGL OBJ Viewer | Assignment 1",
                              WS_OVERLAPPEDWINDOW, x, y, windowWidth, windowHeight,
                              nullptr, nullptr, instance, this);
    if (!window_) {
        showFatalError("Could not create the application window.");
        return false;
    }

    DragAcceptFiles(window_, TRUE);

    RECT clientRect{};
    GetClientRect(window_, &clientRect);
    width_ = std::max(1, static_cast<int>(clientRect.right - clientRect.left));
    height_ = std::max(1, static_cast<int>(clientRect.bottom - clientRect.top));
    return true;
}

bool Viewer::createOpenGL() {
    deviceContext_ = GetDC(window_);
    if (!deviceContext_) {
        showFatalError("Could not obtain the window device context.");
        return false;
    }

    HWND dummyWindow = CreateWindowExA(0, kWindowClass, "dummy", WS_OVERLAPPEDWINDOW,
                                       0, 0, 1, 1, nullptr, nullptr, instance_, nullptr);
    if (!dummyWindow) {
        showFatalError("Could not create the OpenGL bootstrap window.");
        return false;
    }
    HDC dummyContext = GetDC(dummyWindow);

    PIXELFORMATDESCRIPTOR pixelFormat{};
    pixelFormat.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pixelFormat.nVersion = 1;
    pixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormat.iPixelType = PFD_TYPE_RGBA;
    pixelFormat.cColorBits = 32;
    pixelFormat.cAlphaBits = 8;
    pixelFormat.cDepthBits = 24;
    pixelFormat.cStencilBits = 8;
    pixelFormat.iLayerType = PFD_MAIN_PLANE;

    const int dummyFormat = ChoosePixelFormat(dummyContext, &pixelFormat);
    if (!dummyFormat || !SetPixelFormat(dummyContext, dummyFormat, &pixelFormat)) {
        showFatalError("Could not set a pixel format for the OpenGL bootstrap context.");
        return false;
    }
    HGLRC dummyRenderContext = wglCreateContext(dummyContext);
    if (!dummyRenderContext || !wglMakeCurrent(dummyContext, dummyRenderContext)) {
        showFatalError("Could not create the OpenGL bootstrap context.");
        return false;
    }

    auto choosePixelFormat = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(
        wglGetProcAddress("wglChoosePixelFormatARB"));
    auto createContext = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
        wglGetProcAddress("wglCreateContextAttribsARB"));
    auto swapInterval = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
        wglGetProcAddress("wglSwapIntervalEXT"));

    int format = 0;
    UINT formatCount = 0;
    if (choosePixelFormat) {
        const int attributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB, 32,
            WGL_ALPHA_BITS_ARB, 8,
            WGL_DEPTH_BITS_ARB, 24,
            WGL_STENCIL_BITS_ARB, 8,
            WGL_SAMPLE_BUFFERS_ARB, 1,
            WGL_SAMPLES_ARB, 4,
            0
        };
        choosePixelFormat(deviceContext_, attributes, nullptr, 1, &format, &formatCount);
    }
    if (!format || formatCount == 0) {
        format = ChoosePixelFormat(deviceContext_, &pixelFormat);
    }
    PIXELFORMATDESCRIPTOR chosenFormat{};
    DescribePixelFormat(deviceContext_, format, sizeof(PIXELFORMATDESCRIPTOR), &chosenFormat);
    if (!format || !SetPixelFormat(deviceContext_, format, &chosenFormat)) {
        showFatalError("Could not set the OpenGL pixel format.");
        return false;
    }

    if (createContext) {
        const int contextAttributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        renderContext_ = createContext(deviceContext_, nullptr, contextAttributes);
        if (!renderContext_) {
            const int fallbackAttributes[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                0
            };
            renderContext_ = createContext(deviceContext_, nullptr, fallbackAttributes);
        }
    }
    if (!renderContext_) {
        renderContext_ = wglCreateContext(deviceContext_);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummyRenderContext);
    ReleaseDC(dummyWindow, dummyContext);
    DestroyWindow(dummyWindow);

    if (!renderContext_ || !wglMakeCurrent(deviceContext_, renderContext_)) {
        showFatalError("Could not activate an OpenGL 3.3 context.");
        return false;
    }

    if (!gl::loadFunctions()) {
        showFatalError(gl::lastError());
        return false;
    }

    const char* versionText = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    int major = 0;
    int minor = 0;
    if (versionText) {
        std::sscanf(versionText, "%d.%d", &major, &minor);
    }
    if (major < 3 || (major == 3 && minor < 3)) {
        showFatalError("OpenGL 3.3 is required. The driver reported: " +
                       std::string(versionText ? versionText : "unknown"));
        return false;
    }

    if (swapInterval) {
        swapInterval(1);
    }
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    return true;
}

void Viewer::update(float deltaSeconds) {
    const bool fast = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const float moveSpeed = (fast ? 3.2f : 1.35f) * deltaSeconds;
    const og::Vec3 screenRight = camera_.right();
    const og::Vec3 screenUp = camera_.up();
    const og::Vec3 screenForward = camera_.forward();
    og::Vec3 translationDelta{0.0f, 0.0f, 0.0f};
    if (keys_[VK_LEFT]) translationDelta -= screenRight;
    if (keys_[VK_RIGHT]) translationDelta += screenRight;
    if (keys_[VK_UP]) translationDelta += screenUp;
    if (keys_[VK_DOWN]) translationDelta -= screenUp;
    if (keys_[VK_PRIOR]) translationDelta += screenForward;
    if (keys_[VK_NEXT]) translationDelta -= screenForward;
    transform_.translation += translationDelta * moveSpeed;

    const float rotateSpeed = (fast ? 2.8f : 1.55f) * deltaSeconds;
    if (keys_['W']) transform_.rotation.x += rotateSpeed;
    if (keys_['S']) transform_.rotation.x -= rotateSpeed;
    if (keys_['A']) transform_.rotation.y -= rotateSpeed;
    if (keys_['D']) transform_.rotation.y += rotateSpeed;
    if (keys_['Q']) transform_.rotation.z += rotateSpeed;
    if (keys_['E']) transform_.rotation.z -= rotateSpeed;

    if (keys_[VK_OEM_PLUS] || keys_[VK_ADD]) {
        transform_.scale *= std::exp(1.15f * deltaSeconds);
    }
    if (keys_[VK_OEM_MINUS] || keys_[VK_SUBTRACT]) {
        transform_.scale *= std::exp(-1.15f * deltaSeconds);
    }
    transform_.scale = og::clampf(transform_.scale, 0.25f, 4.0f);

    auto wrapAngle = [](float angle) {
        while (angle > og::kPi) angle -= 2.0f * og::kPi;
        while (angle < -og::kPi) angle += 2.0f * og::kPi;
        return angle;
    };
    transform_.rotation.x = wrapAngle(transform_.rotation.x);
    transform_.rotation.y = wrapAngle(transform_.rotation.y);
    transform_.rotation.z = wrapAngle(transform_.rotation.z);

    if (wheelFlash_ > 0.0f) {
        wheelFlash_ = std::max(0.0f, wheelFlash_ - deltaSeconds);
    }
}

void Viewer::render() {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    renderer_.draw(model_, camera_, transform_, aspect, showGrid_, showWireframe_);
    if (showHud_) {
        drawHud();
    } else {
        axisGizmoVisible_ = false;
    }
    SwapBuffers(deviceContext_);
}

void Viewer::drawHud() {
    ui_.begin(width_, height_);
    const float logicalWidth = static_cast<float>(width_) / ui_.uiScale();
    const float logicalHeight = static_cast<float>(height_) / ui_.uiScale();

    const Color panelBorder{0.18f, 0.52f, 0.65f, 0.50f};
    const Color panelFill{0.018f, 0.030f, 0.045f, 0.78f};
    const Color textColor{0.82f, 0.89f, 0.94f, 1.0f};
    const Color dimColor{0.52f, 0.63f, 0.72f, 1.0f};
    const Color accent{0.20f, 0.80f, 1.00f, 1.0f};
    const Color orange{1.00f, 0.52f, 0.16f, 1.0f};

    auto panel = [&](float x, float y, float width, float height) {
        ui_.rect(x, y, width, height, panelBorder);
        ui_.rect(x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f, panelFill);
    };

    const float labelX = 42.0f;
    const float lineScale = 0.55f;
    const float labelWidth = std::max({ui_.textWidth("MODEL", lineScale),
                                       ui_.textWidth("CAMERA", lineScale),
                                       ui_.textWidth("VIEW", lineScale),
                                       ui_.textWidth("FILE", lineScale),
                                       ui_.textWidth("TEXT", lineScale)});
    const float detailX = std::max(118.0f, labelX + labelWidth + 16.0f);
    const float y0 = 68.0f;
    const float lineStep = std::max(26.0f, ui_.lineHeight(lineScale) * 1.30f);

    char fontLine[96];
    std::snprintf(fontLine, sizeof(fontLine), "[ / ]  FONT SIZE     %3.0f%%    CTRL+WHEEL",
                  hudFontScale_ * 100.0f);
    const char* helpLine = "W S  ROTATE X    A D  ROTATE Y    Q E  ROTATE Z";
    const char* viewLine = "F  WIREFRAME    G  GRID    H  HUD    K  INPUT";
    const char* subtitle = "OPENGL 3.3 CORE / C++17";
    const float titleWidth = ui_.textWidth("OBJ VIEWER / ASSIGNMENT 1", 0.84f);
    const float subtitleWidth = ui_.textWidth(subtitle, 0.46f);
    const float titleEnd = 42.0f + titleWidth + 18.0f + subtitleWidth;
    const float helpWidth = std::max({ui_.textWidth(helpLine, lineScale),
                                      ui_.textWidth(fontLine, lineScale),
                                      ui_.textWidth(viewLine, lineScale)});
    const float panelWidth = std::max({690.0f, titleEnd + 24.0f, detailX + helpWidth + 26.0f});
    const float panelHeight = (y0 - 20.0f) + 7.0f * lineStep +
                              ui_.lineHeight(lineScale) + 12.0f;

    panel(24.0f, 20.0f, panelWidth, panelHeight);
    ui_.rect(24.0f, 20.0f, 4.0f, panelHeight, accent);
    ui_.text(42.0f, 30.0f, 0.84f, textColor, "OBJ VIEWER / ASSIGNMENT 1");
    ui_.text(42.0f + titleWidth + 18.0f, 35.0f, 0.46f, dimColor, subtitle);

    auto labeledLine = [&](int index, const char* label, const char* text) {
        const float y = y0 + index * lineStep;
        ui_.text(labelX, y, lineScale, orange, label);
        ui_.text(detailX, y, lineScale, textColor, text);
    };
    labeledLine(0, "MODEL", "ARROWS / PGUP PGDN  MOVE");
    ui_.text(detailX, y0 + lineStep, lineScale, textColor,
             "W S  ROTATE X    A D  ROTATE Y    Q E  ROTATE Z");
    ui_.text(detailX, y0 + 2.0f * lineStep, lineScale, textColor,
             "+ / -  SCALE      0  RESET MODEL");
    labeledLine(3, "CAMERA", "LMB DRAG  ORBIT / LOOKAT    RMB DRAG  PAN");
    ui_.text(detailX, y0 + 4.0f * lineStep, lineScale, textColor,
             "WHEEL  ZOOM      R  RESET ALL      ESC  QUIT");
    labeledLine(5, "VIEW", viewLine);
    labeledLine(6, "FILE", "O  OPEN OBJ        DRAG & DROP");
    labeledLine(7, "TEXT", fontLine);

    if (logicalWidth > 1080.0f) {
        char triangleText[96];
        std::snprintf(triangleText, sizeof(triangleText), "%u TRIANGLES / %zu MATERIALS",
                      model_.triangleCount(), model_.materials().size());
        const float badgeTitleWidth = ui_.textWidth("OPENGL 3.3 CORE", 0.54f);
        const float badgeAssetWidth = ui_.textWidth(assetName_, 0.50f);
        const float badgeStatWidth = ui_.textWidth(triangleText, 0.40f);
        const float badgeWidth = std::max({262.0f,
                                           28.0f + std::max({badgeTitleWidth, badgeAssetWidth,
                                                             badgeStatWidth})});
        const float badgeX = logicalWidth - badgeWidth - 24.0f;
        const float titleHeight = ui_.lineHeight(0.54f);
        const float assetHeight = ui_.lineHeight(0.50f);
        const float statHeight = ui_.lineHeight(0.40f);
        const float badgeTitleY = 32.0f;
        const float badgeAssetY = badgeTitleY + titleHeight + 8.0f;
        const float badgeStatY = badgeAssetY + assetHeight + 7.0f;
        const float badgeHeight = badgeStatY + statHeight + 10.0f - 20.0f;

        if (badgeX >= 24.0f + panelWidth + 12.0f) {
            panel(badgeX, 20.0f, badgeWidth, badgeHeight);
            ui_.rect(badgeX, 20.0f, badgeWidth, 3.0f, accent);
            ui_.text(badgeX + 14.0f, badgeTitleY, 0.54f, accent, "OPENGL 3.3 CORE");
            ui_.text(badgeX + 14.0f, badgeAssetY, 0.50f, textColor, assetName_);
            ui_.text(badgeX + 14.0f, badgeStatY, 0.40f, dimColor, triangleText);
        }
    }

    float statusY = logicalHeight - 166.0f;
    char positionText[128];
    std::snprintf(positionText, sizeof(positionText), "POS    X %+6.2f   Y %+6.2f   Z %+6.2f",
                  transform_.translation.x, transform_.translation.y, transform_.translation.z);
    char rotationText[128];
    std::snprintf(rotationText, sizeof(rotationText), "ROT    X %+6.1f   Y %+6.1f   Z %+6.1f  DEG",
                  og::degrees(transform_.rotation.x), og::degrees(transform_.rotation.y),
                  og::degrees(transform_.rotation.z));
    char scaleText[128];
    std::snprintf(scaleText, sizeof(scaleText), "SCALE  %.2f      FPS  %.0f", transform_.scale, fps_);
    char cameraText[128];
    std::snprintf(cameraText, sizeof(cameraText), "CAMERA  YAW %+5.0f  PITCH %+5.0f  DIST %.1f",
                  og::degrees(camera_.yaw()), og::degrees(camera_.pitch()), camera_.distance());
    const float statusWidth = std::max(438.0f, ui_.textWidth(cameraText, 0.46f) + 70.0f);
    bool showStatus = true;
    if (showInputMonitor_) {
        const float monitorLeft = logicalWidth - 542.0f * hudFontScale_ - 24.0f * hudFontScale_;
        if (monitorLeft < statusWidth + 24.0f) {
            const float monitorTop = logicalHeight - 175.0f * hudFontScale_;
            statusY = std::min(statusY, monitorTop - 27.0f * hudFontScale_ - 146.0f);
            showStatus = statusY >= 300.0f;
        }
    }
    if (showStatus) {
        panel(24.0f, statusY, statusWidth, 142.0f);
        ui_.rect(24.0f, statusY, 4.0f, 142.0f, orange);
        ui_.text(42.0f, statusY + 12.0f, 0.48f, dimColor, "MODEL TRANSFORM");
        ui_.text(42.0f, statusY + 38.0f, 0.46f, textColor, positionText);
        ui_.text(42.0f, statusY + 63.0f, 0.46f, textColor, rotationText);
        ui_.text(42.0f, statusY + 88.0f, 0.46f, textColor, scaleText);
        ui_.text(42.0f, statusY + 113.0f, 0.46f, dimColor, cameraText);
    }

    const char* noteLine = "WHILE ORBITING OR PANNING THE CAMERA";
    const float noteWidth = std::max(336.0f, ui_.textWidth(noteLine, 0.40f) + 28.0f);
    const float noteX = logicalWidth - noteWidth - 24.0f;
    const float noteY = logicalHeight - 82.0f;
    if (!showInputMonitor_) {
        panel(noteX, noteY, noteWidth, 58.0f);
        ui_.text(noteX + 14.0f, noteY + 10.0f, 0.46f, accent, "MODEL STAYS FIXED");
        ui_.text(noteX + 14.0f, noteY + 33.0f, 0.40f, dimColor, noteLine);
    }

    drawAxisGizmo(logicalWidth);
    drawInputMonitor(logicalWidth, logicalHeight);

    ui_.end();
}

void Viewer::drawInputMonitor(float logicalWidth, float logicalHeight) {
    if (!showInputMonitor_) {
        return;
    }

    struct KeyCap {
        const char* label;
        int vk;
        float units;
    };

    const float s = ui_.fontScale();

    const Color panelBorder{0.18f, 0.52f, 0.65f, 1.00f};
    const Color panelFill{0.018f, 0.030f, 0.045f, 1.00f};
    const Color keyFill{0.045f, 0.075f, 0.100f, 1.00f};
    const Color keyActive{0.10f, 0.68f, 0.92f, 1.00f};
    const Color keyBorder{0.18f, 0.32f, 0.40f, 1.00f};
    const Color textColor{0.82f, 0.89f, 0.94f, 1.0f};
    const Color dimColor{0.52f, 0.63f, 0.72f, 1.0f};
    const Color accent{0.20f, 0.80f, 1.00f, 1.0f};

    const float keyWidth = 22.0f * s;
    const float keyHeight = 21.0f * s;
    const float gap = 3.0f * s;
    const float rowStep = 25.0f * s;
    const float monitorWidth = 542.0f * s;
    const float margin = 24.0f * s;
    const float baseX = std::max(margin, logicalWidth - monitorWidth - margin);
    const float baseY = logicalHeight - 175.0f * s;
    const float labelScale = 0.32f;

    auto roundedBox = [&](float x, float y, float width, float height, float radius,
                          const Color& color) {
        const float r = std::min(radius, std::min(width, height) * 0.5f);
        ui_.circle(x + r, y + r, r, color, 24);
        ui_.circle(x + width - r, y + height - r, r, color, 24);
        ui_.rect(x + r, y, width - 2.0f * r, height, color);
        ui_.rect(x, y + r, width, height - 2.0f * r, color);
    };

    ui_.text(baseX, baseY - 27.0f * s, 0.42f, accent, "INPUT MONITOR");
    ui_.text(baseX + 152.0f * s, baseY - 25.0f * s, 0.32f, dimColor, "K  HIDE");

    auto drawKey = [&](const char* label, int vk, float x, float y, float width) {
        const bool pressed = vk >= 0 && vk < 256 && keys_[vk];
        const Color fill = pressed ? keyActive : keyFill;
        const Color border = pressed ? Color{0.55f, 0.95f, 1.0f, 1.0f} : keyBorder;
        ui_.rect(x, y, width, keyHeight, border);
        ui_.rect(x + 1.0f * s, y + 1.0f * s, width - 2.0f * s, keyHeight - 2.0f * s, fill);
        const float textX = x + std::max(2.0f * s, (width - ui_.textWidth(label, labelScale)) * 0.5f);
        ui_.text(textX, y + 4.0f * s, labelScale,
                 pressed ? Color{0.01f, 0.05f, 0.07f, 1.0f} : textColor, label);
    };

    auto drawRow = [&](std::initializer_list<KeyCap> caps, float y) {
        float x = baseX;
        for (const KeyCap& cap : caps) {
            const float width = cap.units * keyWidth;
            drawKey(cap.label, cap.vk, x, y, width);
            x += width + gap;
        }
    };

    drawRow({{"ESC", VK_ESCAPE, 1.2f},
             {"1", '1', 1.0f}, {"2", '2', 1.0f}, {"3", '3', 1.0f}, {"4", '4', 1.0f},
             {"5", '5', 1.0f}, {"6", '6', 1.0f}, {"7", '7', 1.0f}, {"8", '8', 1.0f},
             {"9", '9', 1.0f}, {"0", '0', 1.0f}, {"-", VK_OEM_MINUS, 1.0f},
             {"=", VK_OEM_PLUS, 1.0f}}, baseY);
    drawRow({{"TAB", VK_TAB, 1.4f},
             {"Q", 'Q', 1.0f}, {"W", 'W', 1.0f}, {"E", 'E', 1.0f}, {"R", 'R', 1.0f},
             {"T", 'T', 1.0f}, {"Y", 'Y', 1.0f}, {"U", 'U', 1.0f}, {"I", 'I', 1.0f},
             {"O", 'O', 1.0f}, {"P", 'P', 1.0f}, {"[", VK_OEM_4, 1.0f},
             {"]", VK_OEM_6, 1.0f}}, baseY + rowStep);
    drawRow({{"CAPS", VK_CAPITAL, 1.5f},
             {"A", 'A', 1.0f}, {"S", 'S', 1.0f}, {"D", 'D', 1.0f}, {"F", 'F', 1.0f},
             {"G", 'G', 1.0f}, {"H", 'H', 1.0f}, {"J", 'J', 1.0f}, {"K", 'K', 1.0f},
             {"L", 'L', 1.0f}, {";", VK_OEM_1, 1.0f}, {"'", VK_OEM_7, 1.0f}},
            baseY + 2.0f * rowStep);
    drawRow({{"SHIFT", VK_SHIFT, 2.0f},
             {"Z", 'Z', 1.0f}, {"X", 'X', 1.0f}, {"C", 'C', 1.0f}, {"V", 'V', 1.0f},
             {"B", 'B', 1.0f}, {"N", 'N', 1.0f}, {"M", 'M', 1.0f},
             {",", VK_OEM_COMMA, 1.0f}, {".", VK_OEM_PERIOD, 1.0f},
             {"/", VK_OEM_2, 1.0f}, {"SHIFT", VK_SHIFT, 2.0f}},
            baseY + 3.0f * rowStep);
    drawRow({{"CTRL", VK_CONTROL, 1.5f}, {"ALT", VK_MENU, 1.3f},
             {"SPACE", VK_SPACE, 5.0f}, {"ALT", VK_MENU, 1.3f},
             {"CTRL", VK_CONTROL, 1.5f}}, baseY + 4.0f * rowStep);

    const float navX = baseX + 370.0f * s;
    drawKey("PGUP", VK_PRIOR, navX, baseY, 48.0f * s);
    drawKey("PGDN", VK_NEXT, navX, baseY + rowStep, 48.0f * s);
    drawKey("UP", VK_UP, navX + 26.0f * s, baseY + 2.0f * rowStep, 22.0f * s);
    drawKey("LT", VK_LEFT, navX, baseY + 3.0f * rowStep, 22.0f * s);
    drawKey("DN", VK_DOWN, navX + 26.0f * s, baseY + 3.0f * rowStep, 22.0f * s);
    drawKey("RT", VK_RIGHT, navX + 52.0f * s, baseY + 3.0f * rowStep, 22.0f * s);

    const float mouseX = baseX + 470.0f * s;
    const float mouseY = baseY + 8.0f * s;
    const float mouseWidth = 72.0f * s;
    const float mouseHeight = 132.0f * s;
    const float upperHeight = 50.0f * s;
    const float splitY = mouseY + upperHeight;
    const float centerX = mouseX + mouseWidth * 0.5f;
    const bool leftDown = leftDragging_ || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rightDown = rightDragging_ || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    // Outer shell: lower part is the palm rest, upper part contains the two buttons.
    roundedBox(mouseX, mouseY, mouseWidth, mouseHeight, mouseWidth * 0.5f, panelBorder);
    roundedBox(mouseX + 1.5f * s, mouseY + 1.5f * s, mouseWidth - 3.0f * s,
               mouseHeight - 3.0f * s, mouseWidth * 0.5f - 1.5f * s, panelFill);
    ui_.rect(mouseX + 3.0f * s, splitY, mouseWidth - 6.0f * s,
             std::max(1.0f, 1.2f * s), keyBorder);

    // The upper area is defined by the dividing lines, not by boxed buttons.
    if (leftDown) {
        ui_.rect(centerX - 22.0f * s, mouseY + 40.0f * s, 16.0f * s,
                 std::max(1.0f, 1.8f * s), keyActive);
    }
    if (rightDown) {
        ui_.rect(centerX + 6.0f * s, mouseY + 40.0f * s, 16.0f * s,
                 std::max(1.0f, 1.8f * s), keyActive);
    }
    ui_.rect(centerX - 0.7f * s, mouseY + 3.0f * s, std::max(1.0f, 1.4f * s),
             upperHeight - 6.0f * s, keyBorder);
    roundedBox(centerX - 3.5f * s, mouseY + 11.0f * s, 7.0f * s, 22.0f * s,
               3.5f * s, keyBorder);
    roundedBox(centerX - 2.6f * s, mouseY + 12.0f * s, 5.2f * s, 20.0f * s,
               2.6f * s, wheelFlash_ > 0.0f ? accent : dimColor);

    ui_.text(centerX - 15.0f * s, mouseY + 33.0f * s, 0.36f,
             leftDown ? accent : textColor, "L");
    ui_.text(centerX + 10.0f * s, mouseY + 33.0f * s, 0.36f,
             rightDown ? accent : textColor, "R");
}

void Viewer::setHudFontScale(float scale) {
    hudFontScale_ = og::clampf(scale, 0.75f, 1.8f);
    ui_.setFontScale(hudFontScale_);
}

void Viewer::drawAxisGizmo(float logicalWidth) {
    const float uiScale = ui_.uiScale();
    const float centerX = logicalWidth - 96.0f;
    const float centerY = 188.0f + std::max(0.0f, hudFontScale_ - 1.0f) * 32.0f;
    const float radius = 54.0f;

    ui_.circle(centerX, centerY, 64.0f, Color{0.16f, 0.42f, 0.54f, 0.38f}, 48);
    ui_.circle(centerX, centerY, 61.0f, Color{0.010f, 0.022f, 0.034f, 0.82f}, 48);

    const og::Vec3 axes[3] = {
        og::Vec3{1.0f, 0.0f, 0.0f},
        og::Vec3{0.0f, 1.0f, 0.0f},
        og::Vec3{0.0f, 0.0f, 1.0f}
    };
    const Color colors[3] = {
        Color{1.00f, 0.28f, 0.24f, 0.96f},
        Color{0.36f, 0.92f, 0.42f, 0.96f},
        Color{0.28f, 0.62f, 1.00f, 0.96f}
    };
    const char* labels[3] = {"X", "Y", "Z"};
    const og::Vec3 right = camera_.right();
    const og::Vec3 up = camera_.up();

    for (int i = 0; i < 3; ++i) {
        float sx = og::dot(axes[i], right);
        float sy = -og::dot(axes[i], up);
        const float projected = std::sqrt(sx * sx + sy * sy);
        if (projected < 0.08f) {
            if (i == 0) {
                sx = camera_.position().x >= 0.0f ? 1.0f : -1.0f;
                sy = 0.0f;
            } else if (i == 1) {
                sx = 0.0f;
                sy = camera_.position().y >= 0.0f ? -1.0f : 1.0f;
            } else {
                sx = 0.0f;
                sy = 1.0f;
            }
        } else {
            sx /= projected;
            sy /= projected;
        }

        const float visualRadius = radius * (0.74f + 0.26f * std::min(projected, 1.0f));
        const float endX = centerX + sx * visualRadius;
        const float endY = centerY + sy * visualRadius;

        if (viewAxis_ == i && viewSign_ > 0) {
            ui_.circle(endX, endY, 13.0f,
                       Color{colors[i].r, colors[i].g, colors[i].b, 0.20f}, 24);
        }

        Color faded = colors[i];
        faded.a = 0.30f;
        ui_.line(centerX, centerY, centerX - sx * visualRadius * 0.32f,
                 centerY - sy * visualRadius * 0.32f, 1.2f, faded);
        ui_.line(centerX, centerY, endX, endY, 2.2f, colors[i]);
        ui_.circle(endX, endY, 8.0f, colors[i], 24);
        ui_.circle(endX, endY, 3.0f, Color{0.02f, 0.04f, 0.05f, 0.95f}, 16);

        const float labelCenterX = endX + sx * 13.0f;
        const float labelCenterY = endY + sy * 13.0f;
        const float labelWidth = ui_.textWidth(labels[i], 0.48f);
        const float labelHeight = ui_.lineHeight(0.48f);
        ui_.text(labelCenterX - labelWidth * 0.5f, labelCenterY - labelHeight * 0.5f,
                 0.48f, colors[i], labels[i]);

        axisHitX_[i] = endX * uiScale;
        axisHitY_[i] = endY * uiScale;
    }

    const float hintWidth = ui_.textWidth("CLICK AXIS", 0.36f);
    ui_.text(centerX - hintWidth * 0.5f, centerY + 68.0f, 0.36f,
             Color{0.48f, 0.60f, 0.70f, 0.92f}, "CLICK AXIS");

    const char* negativeLabels[3] = {"-X", "-Y", "-Z"};
    const float chipWidth = 38.0f;
    const float chipHeight = 20.0f;
    const float chipGap = 7.0f;
    const float totalWidth = chipWidth * 3.0f + chipGap * 2.0f;
    const float chipStartX = centerX - totalWidth * 0.5f;
    const float chipY = centerY + 68.0f + ui_.lineHeight(0.36f) + 8.0f;
    for (int i = 0; i < 3; ++i) {
        const float chipX = chipStartX + static_cast<float>(i) * (chipWidth + chipGap);
        const bool active = (viewAxis_ == i && viewSign_ < 0);
        Color border = colors[i];
        border.a = active ? 0.95f : 0.42f;
        const Color fill = active ? Color{0.030f, 0.080f, 0.110f, 0.94f}
                                  : Color{0.012f, 0.025f, 0.038f, 0.86f};
        ui_.rect(chipX, chipY, chipWidth, chipHeight, border);
        ui_.rect(chipX + 1.0f, chipY + 1.0f, chipWidth - 2.0f, chipHeight - 2.0f, fill);
        const float chipTextWidth = ui_.textWidth(negativeLabels[i], 0.40f);
        const float chipTextHeight = ui_.lineHeight(0.40f);
        const Color chipTextColor{0.90f, 0.94f, 0.97f, 0.98f};
        ui_.text(chipX + (chipWidth - chipTextWidth) * 0.5f,
                 chipY + (chipHeight - chipTextHeight) * 0.5f, 0.40f,
                 chipTextColor, negativeLabels[i]);
        negativeHitX_[i] = (chipX + chipWidth * 0.5f) * uiScale;
        negativeHitY_[i] = (chipY + chipHeight * 0.5f) * uiScale;
    }
    negativeHitHalfWidth_ = chipWidth * 0.5f * uiScale;
    negativeHitHalfHeight_ = chipHeight * 0.5f * uiScale;

    axisGizmoCenterX_ = centerX * uiScale;
    axisGizmoCenterY_ = centerY * uiScale;
    axisGizmoRadius_ = 76.0f * uiScale;
    axisHitRadius_ = 22.0f * uiScale;
    axisGizmoVisible_ = true;
}

bool Viewer::hitAxisGizmo(int x, int y, int& axis, int& sign) const {
    axis = -1;
    sign = 1;
    if (!axisGizmoVisible_) {
        return false;
    }

    const float mouseX = static_cast<float>(x);
    const float mouseY = static_cast<float>(y);
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(mouseX - negativeHitX_[i]) <= negativeHitHalfWidth_ &&
            std::fabs(mouseY - negativeHitY_[i]) <= negativeHitHalfHeight_) {
            axis = i;
            sign = -1;
            return true;
        }
    }

    const float dx = mouseX - axisGizmoCenterX_;
    const float dy = mouseY - axisGizmoCenterY_;
    if (dx * dx + dy * dy > axisGizmoRadius_ * axisGizmoRadius_) {
        return false;
    }

    float bestDistance = axisHitRadius_ * axisHitRadius_;
    for (int i = 0; i < 3; ++i) {
        const float ax = mouseX - axisHitX_[i];
        const float ay = mouseY - axisHitY_[i];
        const float distance = ax * ax + ay * ay;
        if (distance < bestDistance) {
            bestDistance = distance;
            axis = i;
        }
    }
    return true;
}

void Viewer::run() {
    LARGE_INTEGER frequency{};
    LARGE_INTEGER counter{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    lastTime_ = static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);

    MSG message{};
    while (running_) {
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (!running_) {
            break;
        }

        QueryPerformanceCounter(&counter);
        const double now = static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
        float delta = static_cast<float>(now - lastTime_);
        lastTime_ = now;
        delta = og::clampf(delta, 0.0001f, 0.05f);

        fpsTimer_ += delta;
        fps_ = fps_ * 0.92f + (1.0f / delta) * 0.08f;
        if (fpsTimer_ > 0.5f) {
            fpsTimer_ = 0.0f;
            char title[192];
            std::snprintf(title, sizeof(title), "OpenGL OBJ Viewer | %s | %.0f FPS",
                          assetName_.c_str(), fps_);
            SetWindowTextA(window_, title);
        }

        if (IsIconic(window_)) {
            Sleep(16);
            continue;
        }
        update(delta);
        render();
    }
}

void Viewer::shutdown() {
    if (renderContext_ && deviceContext_) {
        wglMakeCurrent(deviceContext_, renderContext_);
        ui_.shutdown();
        renderer_.shutdown();
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(renderContext_);
        renderContext_ = nullptr;
    }
    if (deviceContext_ && window_) {
        ReleaseDC(window_, deviceContext_);
        deviceContext_ = nullptr;
    }
    if (window_) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

LRESULT CALLBACK Viewer::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        auto* viewer = reinterpret_cast<Viewer*>(create->lpCreateParams);
        SetWindowLongPtrA(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(viewer));
        if (viewer) {
            viewer->window_ = window;
        }
    }
    auto* viewer = reinterpret_cast<Viewer*>(GetWindowLongPtrA(window, GWLP_USERDATA));
    if (viewer) {
        return viewer->handleMessage(window, message, wParam, lParam);
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

LRESULT Viewer::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DROPFILES: {
        const HDROP drop = reinterpret_cast<HDROP>(wParam);
        const UINT length = DragQueryFileA(drop, 0, nullptr, 0);
        if (length > 0) {
            std::vector<char> path(static_cast<std::size_t>(length) + 1, 0);
            if (DragQueryFileA(drop, 0, path.data(), length + 1) > 0) {
                loadModelFile(path.data());
            }
        }
        DragFinish(drop);
        return 0;
    }
    case WM_SIZE:
        width_ = std::max(1, static_cast<int>(LOWORD(lParam)));
        height_ = std::max(1, static_cast<int>(HIWORD(lParam)));
        ui_.setUiScale(interfaceScale(width_, height_));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam < 256) {
            keys_[wParam] = true;
        }
        if (!(lParam & 0x40000000)) {
            if (wParam == VK_ESCAPE) {
                running_ = false;
                PostQuitMessage(0);
            } else if (wParam == VK_OEM_4) {
                setHudFontScale(hudFontScale_ - 0.05f);
            } else if (wParam == VK_OEM_6) {
                setHudFontScale(hudFontScale_ + 0.05f);
            } else if (wParam == '0' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                setHudFontScale(1.0f);
            } else if (wParam == 'O') {
                openObjDialog();
            } else if (wParam == 'R') {
                camera_.reset();
                viewAxis_ = -1;
                transform_.reset();
            } else if (wParam == '0') {
                transform_.reset();
            } else if (wParam == 'F') {
                showWireframe_ = !showWireframe_;
            } else if (wParam == 'G') {
                showGrid_ = !showGrid_;
            } else if (wParam == 'H') {
                showHud_ = !showHud_;
            } else if (wParam == 'K') {
                showInputMonitor_ = !showInputMonitor_;
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256) {
            keys_[wParam] = false;
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const int mouseX = GET_X_LPARAM(lParam);
        const int mouseY = GET_Y_LPARAM(lParam);
        int axis = -1;
        int sign = 1;
        if (hitAxisGizmo(mouseX, mouseY, axis, sign)) {
            if (axis >= 0) {
                camera_.setAxisView(axis, sign);
                viewAxis_ = axis;
                viewSign_ = sign;
            }
            return 0;
        }
        leftDragging_ = true;
        SetCapture(window);
        lastMouse_.x = mouseX;
        lastMouse_.y = mouseY;
        return 0;
    }
    case WM_LBUTTONUP:
        leftDragging_ = false;
        if (!rightDragging_) {
            ReleaseCapture();
        }
        return 0;
    case WM_RBUTTONDOWN:
        rightDragging_ = true;
        SetCapture(window);
        lastMouse_.x = GET_X_LPARAM(lParam);
        lastMouse_.y = GET_Y_LPARAM(lParam);
        return 0;
    case WM_RBUTTONUP:
        rightDragging_ = false;
        if (!leftDragging_) {
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (leftDragging_) {
            camera_.orbit(static_cast<float>(x - lastMouse_.x),
                          static_cast<float>(y - lastMouse_.y));
            viewAxis_ = -1;
        } else if (rightDragging_) {
            camera_.pan(static_cast<float>(x - lastMouse_.x),
                        static_cast<float>(y - lastMouse_.y));
        }
        lastMouse_.x = x;
        lastMouse_.y = y;
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const float wheelSteps = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / 120.0f;
        wheelFlash_ = 0.20f;
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            setHudFontScale(hudFontScale_ + (wheelSteps > 0.0f ? 0.05f : -0.05f));
            return 0;
        }
        camera_.zoom(wheelSteps);
        return 0;
    }
    case WM_KILLFOCUS:
        std::fill(std::begin(keys_), std::end(keys_), false);
        leftDragging_ = false;
        rightDragging_ = false;
        ReleaseCapture();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 760;
        info->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_CLOSE:
        running_ = false;
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        running_ = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(window, message, wParam, lParam);
    }
}

void Viewer::writeLog(const std::string& message) {
    if (logPath_.empty()) {
        return;
    }
    std::ofstream log(logPath_, std::ios::app);
    if (log) {
        log << message << '\n';
    }
}

void Viewer::showFatalError(const std::string& message) {
    writeLog("ERROR: " + message);
    MessageBoxA(window_, message.c_str(), "OpenGL OBJ Viewer", MB_OK | MB_ICONERROR);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
    (void)previousInstance;
    (void)commandLine;
    Viewer viewer;
    if (!viewer.initialize(instance, showCommand)) {
        viewer.shutdown();
        return 1;
    }
    viewer.run();
    viewer.shutdown();
    return 0;
}
