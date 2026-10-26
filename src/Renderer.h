#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Camera.h"
#include "GLFunctions.h"
#include "Math.h"
#include "ModelTransform.h"
#include "ObjModel.h"

class Renderer {
public:
    bool init(std::string& error);
    void shutdown();

    void buildModelBuffers(const ObjModel& model);
    void draw(const ObjModel& model, const Camera& camera, const ModelTransform& transform,
              float aspect, bool showGrid, bool showWireframe);

private:
    struct Program {
        GLuint id = 0;
        bool build(const char* label, const char* vertexSource, const char* fragmentSource,
                   std::string& error);
        void destroy();
        GLint uniform(const char* name) const;
    };

    void drawBackground(float aspect);
    void drawFloor(const Camera& camera, const ModelTransform& transform, const ObjModel& model,
                   float aspect);
    void drawMesh(const ObjModel& model, const Camera& camera, const ModelTransform& transform,
                  float aspect);
    void drawWireframe(const ObjModel& model, const Camera& camera, const ModelTransform& transform,
                       float aspect);

    Program backgroundProgram_;
    Program floorProgram_;
    Program meshProgram_;
    Program wireProgram_;

    GLuint backgroundVao_ = 0;
    GLuint floorVao_ = 0;
    GLuint floorVbo_ = 0;
    GLuint meshVao_ = 0;
    GLuint meshVbo_ = 0;
    GLuint meshEbo_ = 0;

    GLuint loadTexture(const std::string& path);
    std::vector<GLuint> materialTextures_;
    std::unordered_map<std::string, GLuint> textureCache_;
    std::uintptr_t gdiplusToken_ = 0;
};
