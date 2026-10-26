#include "TextRenderer.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>

#include "Shaders.h"

namespace {

constexpr int kAtlasColumns = 12;
constexpr int kAtlasRows = 8;
constexpr int kCellWidth = 16;
constexpr int kCellHeight = 24;
constexpr int kAtlasWidth = kAtlasColumns * kCellWidth;
constexpr int kAtlasHeight = kAtlasRows * kCellHeight;

GLuint compileProgram(const char* vertexSource, const char* fragmentSource, std::string& error) {
    auto compile = [&](GLenum type, const char* source) -> GLuint {
        const GLuint shader = gl::CreateShader(type);
        gl::ShaderSource(shader, 1, &source, nullptr);
        gl::CompileShader(shader);
        GLint success = GL_FALSE;
        gl::GetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE) {
            GLint length = 0;
            gl::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            gl::GetShaderInfoLog(shader, length, nullptr, log.data());
            error = "UI shader compilation failed:\n" + log;
            gl::DeleteShader(shader);
            return 0;
        }
        return shader;
    };

    const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource);
    if (!vertex) {
        return 0;
    }
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragment) {
        gl::DeleteShader(vertex);
        return 0;
    }
    const GLuint program = gl::CreateProgram();
    gl::AttachShader(program, vertex);
    gl::AttachShader(program, fragment);
    gl::LinkProgram(program);
    gl::DeleteShader(vertex);
    gl::DeleteShader(fragment);

    GLint success = GL_FALSE;
    gl::GetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint length = 0;
        gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
        gl::GetProgramInfoLog(program, length, nullptr, log.data());
        error = "UI program link failed:\n" + log;
        gl::DeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace

bool TextRenderer::init(float uiScale, std::string& error) {
    uiScale_ = std::max(uiScale, 0.75f);
    colorProgram_ = compileProgram(shaders::kUiColorVert, shaders::kUiColorFrag, error);
    if (!colorProgram_) {
        return false;
    }
    textProgram_ = compileProgram(shaders::kUiTextVert, shaders::kUiTextFrag, error);
    if (!textProgram_) {
        return false;
    }

    gl::GenVertexArrays(1, &colorVao_);
    gl::GenBuffers(1, &colorVbo_);
    gl::BindVertexArray(colorVao_);
    gl::BindBuffer(GL_ARRAY_BUFFER, colorVbo_);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                            reinterpret_cast<void*>(offsetof(ColorVertex, x)));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                            reinterpret_cast<void*>(offsetof(ColorVertex, r)));

    gl::GenVertexArrays(1, &textVao_);
    gl::GenBuffers(1, &textVbo_);
    gl::BindVertexArray(textVao_);
    gl::BindBuffer(GL_ARRAY_BUFFER, textVbo_);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                            reinterpret_cast<void*>(offsetof(TextVertex, x)));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                            reinterpret_cast<void*>(offsetof(TextVertex, u)));
    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                            reinterpret_cast<void*>(offsetof(TextVertex, r)));
    gl::BindVertexArray(0);

    createFontAtlas(error);
    if (!fontTexture_) {
        return false;
    }
    return true;
}

void TextRenderer::createFontAtlas(std::string& error) {
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kAtlasWidth;
    info.bmiHeader.biHeight = -kAtlasHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap || !pixels) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        error = "Could not create the font atlas bitmap.";
        return;
    }
    std::memset(pixels, 0, static_cast<std::size_t>(kAtlasWidth * kAtlasHeight * 4));

    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    HFONT font = CreateFontA(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             FF_DONTCARE, "Consolas");
    const bool ownedFont = font != nullptr;
    if (!font) {
        font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    HGDIOBJ oldFont = SelectObject(memory, font);
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(255, 255, 255));
    SetTextAlign(memory, TA_LEFT | TA_TOP);

    for (int index = 0; index < kAtlasColumns * kAtlasRows; ++index) {
        const char character = static_cast<char>(index + 32);
        const int column = index % kAtlasColumns;
        const int row = index / kAtlasColumns;
        RECT rect{column * kCellWidth, row * kCellHeight,
                  (column + 1) * kCellWidth, (row + 1) * kCellHeight};
        DrawTextA(memory, &character, 1, &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    GdiFlush();

    glGenTextures(1, &fontTexture_);
    glBindTexture(GL_TEXTURE_2D, fontTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kAtlasWidth, kAtlasHeight, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    SelectObject(memory, oldFont);
    SelectObject(memory, oldBitmap);
    if (ownedFont) {
        DeleteObject(font);
    }
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    if (!fontTexture_) {
        error = "Could not upload the font atlas texture.";
    }
}

void TextRenderer::shutdown() {
    if (fontTexture_) glDeleteTextures(1, &fontTexture_);
    if (textVbo_) gl::DeleteBuffers(1, &textVbo_);
    if (colorVbo_) gl::DeleteBuffers(1, &colorVbo_);
    if (textVao_) gl::DeleteVertexArrays(1, &textVao_);
    if (colorVao_) gl::DeleteVertexArrays(1, &colorVao_);
    if (textProgram_) gl::DeleteProgram(textProgram_);
    if (colorProgram_) gl::DeleteProgram(colorProgram_);
    fontTexture_ = textVbo_ = colorVbo_ = 0;
    textVao_ = colorVao_ = 0;
    textProgram_ = colorProgram_ = 0;
}

void TextRenderer::begin(int viewportWidth, int viewportHeight) {
    viewportWidth_ = std::max(viewportWidth, 1);
    viewportHeight_ = std::max(viewportHeight, 1);
    colorVertices_.clear();
    textVertices_.clear();
}

void TextRenderer::rect(float x, float y, float width, float height, const Color& color) {
    const float x0 = x * uiScale_;
    const float y0 = y * uiScale_;
    const float x1 = (x + width) * uiScale_;
    const float y1 = (y + height) * uiScale_;
    const ColorVertex vertices[6] = {
        {x0, y0, color.r, color.g, color.b, color.a},
        {x1, y0, color.r, color.g, color.b, color.a},
        {x0, y1, color.r, color.g, color.b, color.a},
        {x0, y1, color.r, color.g, color.b, color.a},
        {x1, y0, color.r, color.g, color.b, color.a},
        {x1, y1, color.r, color.g, color.b, color.a}
    };
    colorVertices_.insert(colorVertices_.end(), std::begin(vertices), std::end(vertices));
}

void TextRenderer::line(float x0, float y0, float x1, float y1, float thickness, const Color& color) {
    const float ax = x0 * uiScale_;
    const float ay = y0 * uiScale_;
    const float bx = x1 * uiScale_;
    const float by = y1 * uiScale_;
    const float dx = bx - ax;
    const float dy = by - ay;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.0001f) {
        return;
    }
    const float nx = -dy / length * thickness * 0.5f * uiScale_;
    const float ny = dx / length * thickness * 0.5f * uiScale_;
    const ColorVertex vertices[6] = {
        {ax + nx, ay + ny, color.r, color.g, color.b, color.a},
        {ax - nx, ay - ny, color.r, color.g, color.b, color.a},
        {bx + nx, by + ny, color.r, color.g, color.b, color.a},
        {bx + nx, by + ny, color.r, color.g, color.b, color.a},
        {ax - nx, ay - ny, color.r, color.g, color.b, color.a},
        {bx - nx, by - ny, color.r, color.g, color.b, color.a}
    };
    colorVertices_.insert(colorVertices_.end(), std::begin(vertices), std::end(vertices));
}

void TextRenderer::circle(float centerX, float centerY, float radius, const Color& color,
                          int segments) {
    const float cx = centerX * uiScale_;
    const float cy = centerY * uiScale_;
    const float r = radius * uiScale_;
    const int count = std::max(segments, 8);
    for (int i = 0; i < count; ++i) {
        const float a0 = static_cast<float>(i) / static_cast<float>(count) * 6.28318530718f;
        const float a1 = static_cast<float>(i + 1) / static_cast<float>(count) * 6.28318530718f;
        const ColorVertex vertices[3] = {
            {cx, cy, color.r, color.g, color.b, color.a},
            {cx + std::cos(a0) * r, cy + std::sin(a0) * r, color.r, color.g, color.b, color.a},
            {cx + std::cos(a1) * r, cy + std::sin(a1) * r, color.r, color.g, color.b, color.a}
        };
        colorVertices_.insert(colorVertices_.end(), std::begin(vertices), std::end(vertices));
    }
}

void TextRenderer::addTextQuads(float x, float y, float scale, const Color& color,
                                const std::string& value) {
    const float advance = kCellWidth * scale * 0.95f;
    float cursor = x;
    for (unsigned char raw : value) {
        unsigned char character = raw;
        if (character < 32 || character > 127) {
            character = '?';
        }
        if (character != ' ') {
            const int index = static_cast<int>(character) - 32;
            const int column = index % kAtlasColumns;
            const int row = index / kAtlasColumns;
            const float inset = 0.5f;
            const float u0 = (column * kCellWidth + inset) / static_cast<float>(kAtlasWidth);
            const float v0 = (row * kCellHeight + inset) / static_cast<float>(kAtlasHeight);
            const float u1 = ((column + 1) * kCellWidth - inset) / static_cast<float>(kAtlasWidth);
            const float v1 = ((row + 1) * kCellHeight - inset) / static_cast<float>(kAtlasHeight);
            const float x0 = cursor;
            const float y0 = y;
            const float x1 = cursor + kCellWidth * scale;
            const float y1 = y + kCellHeight * scale;
            const TextVertex vertices[6] = {
                {x0, y0, u0, v0, color.r, color.g, color.b, color.a},
                {x1, y0, u1, v0, color.r, color.g, color.b, color.a},
                {x0, y1, u0, v1, color.r, color.g, color.b, color.a},
                {x0, y1, u0, v1, color.r, color.g, color.b, color.a},
                {x1, y0, u1, v0, color.r, color.g, color.b, color.a},
                {x1, y1, u1, v1, color.r, color.g, color.b, color.a}
            };
            textVertices_.insert(textVertices_.end(), std::begin(vertices), std::end(vertices));
        }
        cursor += advance;
    }
}

void TextRenderer::text(float x, float y, float scale, const Color& color,
                        const std::string& value) {
    std::string upper = value;
    for (char& character : upper) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    const float sx = x * uiScale_;
    const float sy = y * uiScale_;
    const float physicalScale = scale * uiScale_ * fontScale_;
    const float shadowOffset = 1.2f * uiScale_ * fontScale_;
    addTextQuads(sx + shadowOffset, sy + shadowOffset, physicalScale,
                 Color{0.0f, 0.0f, 0.0f, color.a * 0.55f}, upper);
    addTextQuads(sx, sy, physicalScale, color, upper);
}

float TextRenderer::textWidth(const std::string& value, float scale) const {
    return static_cast<float>(value.size()) * kCellWidth * scale * fontScale_ * 0.95f;
}

float TextRenderer::lineHeight(float scale) const {
    return kCellHeight * scale * fontScale_;
}

void TextRenderer::uploadAndDraw() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!colorVertices_.empty()) {
        gl::UseProgram(colorProgram_);
        const float viewport[2] = {static_cast<float>(viewportWidth_),
                                   static_cast<float>(viewportHeight_)};
        gl::Uniform2fv(gl::GetUniformLocation(colorProgram_, "uViewport"), 1, viewport);
        gl::BindVertexArray(colorVao_);
        gl::BindBuffer(GL_ARRAY_BUFFER, colorVbo_);
        gl::BufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(colorVertices_.size() * sizeof(ColorVertex)),
                       colorVertices_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(colorVertices_.size()));
    }

    if (!textVertices_.empty()) {
        gl::UseProgram(textProgram_);
        const float viewport[2] = {static_cast<float>(viewportWidth_),
                                   static_cast<float>(viewportHeight_)};
        gl::Uniform2fv(gl::GetUniformLocation(textProgram_, "uViewport"), 1, viewport);
        gl::ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTexture_);
        gl::Uniform1i(gl::GetUniformLocation(textProgram_, "uFont"), 0);
        gl::BindVertexArray(textVao_);
        gl::BindBuffer(GL_ARRAY_BUFFER, textVbo_);
        gl::BufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(textVertices_.size() * sizeof(TextVertex)),
                       textVertices_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textVertices_.size()));
    }

    gl::BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void TextRenderer::end() {
    uploadAndDraw();
    colorVertices_.clear();
    textVertices_.clear();
}
