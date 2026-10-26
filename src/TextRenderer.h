#pragma once

#include <string>
#include <vector>

#include "GLFunctions.h"

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

class TextRenderer {
public:
    bool init(float uiScale, std::string& error);
    void shutdown();

    void begin(int viewportWidth, int viewportHeight);
    void rect(float x, float y, float width, float height, const Color& color);
    void text(float x, float y, float scale, const Color& color, const std::string& value);
    void line(float x0, float y0, float x1, float y1, float thickness, const Color& color);
    void circle(float centerX, float centerY, float radius, const Color& color, int segments = 32);
    void end();

    float textWidth(const std::string& value, float scale) const;
    float lineHeight(float scale) const;
    float uiScale() const { return uiScale_; }
    void setUiScale(float scale) { uiScale_ = scale < 0.75f ? 0.75f : scale; }
    float fontScale() const { return fontScale_; }
    void setFontScale(float scale) {
        fontScale_ = scale < 0.5f ? 0.5f : (scale > 2.2f ? 2.2f : scale);
    }

private:
    struct ColorVertex {
        float x, y;
        float r, g, b, a;
    };
    struct TextVertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    void createFontAtlas(std::string& error);
    void addTextQuads(float x, float y, float scale, const Color& color, const std::string& value);
    void uploadAndDraw();

    GLuint colorProgram_ = 0;
    GLuint textProgram_ = 0;
    GLuint colorVao_ = 0;
    GLuint colorVbo_ = 0;
    GLuint textVao_ = 0;
    GLuint textVbo_ = 0;
    GLuint fontTexture_ = 0;
    int viewportWidth_ = 1;
    int viewportHeight_ = 1;
    float uiScale_ = 1.0f;
    float fontScale_ = 1.0f;
    std::vector<ColorVertex> colorVertices_;
    std::vector<TextVertex> textVertices_;
};
