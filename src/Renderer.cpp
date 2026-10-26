#define NOMINMAX
#include "Renderer.h"

#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <cwchar>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Shaders.h"

namespace {

std::wstring widenPath(const std::string& path) {
    if (path.empty()) return {};
    UINT codePage = CP_ACP;
    int length = MultiByteToWideChar(codePage, 0, path.c_str(), -1, nullptr, 0);
    if (length <= 1) {
        codePage = CP_UTF8;
        length = MultiByteToWideChar(codePage, 0, path.c_str(), -1, nullptr, 0);
    }
    if (length <= 1) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(codePage, 0, path.c_str(), -1, result.data(), length);
    result.resize(static_cast<std::size_t>(length - 1));
    return result;
}

og::Vec3 toLinear(const og::Vec3& color) {
    return {
        std::pow(og::clampf(color.x, 0.0f, 1.0f), 2.2f),
        std::pow(og::clampf(color.y, 0.0f, 1.0f), 2.2f),
        std::pow(og::clampf(color.z, 0.0f, 1.0f), 2.2f)
    };
}

} // namespace

bool Renderer::Program::build(const char* label, const char* vertexSource,
                              const char* fragmentSource, std::string& error) {
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
            error = std::string(label) + " shader compilation failed:\n" + log;
            gl::DeleteShader(shader);
            return 0;
        }
        return shader;
    };

    const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource);
    if (!vertex) {
        return false;
    }
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragment) {
        gl::DeleteShader(vertex);
        return false;
    }

    id = gl::CreateProgram();
    gl::AttachShader(id, vertex);
    gl::AttachShader(id, fragment);
    gl::LinkProgram(id);
    gl::DeleteShader(vertex);
    gl::DeleteShader(fragment);

    GLint success = GL_FALSE;
    gl::GetProgramiv(id, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint length = 0;
        gl::GetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
        gl::GetProgramInfoLog(id, length, nullptr, log.data());
        error = std::string(label) + " program link failed:\n" + log;
        gl::DeleteProgram(id);
        id = 0;
        return false;
    }
    return true;
}

void Renderer::Program::destroy() {
    if (id) {
        gl::DeleteProgram(id);
        id = 0;
    }
}

GLint Renderer::Program::uniform(const char* name) const {
    return gl::GetUniformLocation(id, name);
}

bool Renderer::init(std::string& error) {
    Gdiplus::GdiplusStartupInput gdiplusInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        error = "GDI+ initialization failed.";
        return false;
    }
    if (!backgroundProgram_.build("Background", shaders::kBackgroundVert, shaders::kBackgroundFrag, error)) {
        return false;
    }
    if (!floorProgram_.build("Floor", shaders::kFloorVert, shaders::kFloorFrag, error)) {
        return false;
    }
    if (!meshProgram_.build("Mesh", shaders::kMeshVert, shaders::kMeshFrag, error)) {
        return false;
    }
    if (!wireProgram_.build("Wireframe", shaders::kMeshVert, shaders::kWireFrag, error)) {
        return false;
    }

    gl::GenVertexArrays(1, &backgroundVao_);

    gl::GenVertexArrays(1, &floorVao_);
    gl::GenBuffers(1, &floorVbo_);
    const float floorVertices[] = {
        -60.0f, 0.0f, -60.0f,
         60.0f, 0.0f, -60.0f,
        -60.0f, 0.0f,  60.0f,
        -60.0f, 0.0f,  60.0f,
         60.0f, 0.0f, -60.0f,
         60.0f, 0.0f,  60.0f
    };
    gl::BindVertexArray(floorVao_);
    gl::BindBuffer(GL_ARRAY_BUFFER, floorVbo_);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    gl::BindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_CULL_FACE);
    return true;
}

void Renderer::shutdown() {
    for (const GLuint texture : materialTextures_) {
        if (texture) glDeleteTextures(1, &texture);
    }
    materialTextures_.clear();
    textureCache_.clear();
    if (meshEbo_) gl::DeleteBuffers(1, &meshEbo_);
    if (meshVbo_) gl::DeleteBuffers(1, &meshVbo_);
    if (floorVbo_) gl::DeleteBuffers(1, &floorVbo_);
    if (meshVao_) gl::DeleteVertexArrays(1, &meshVao_);
    if (floorVao_) gl::DeleteVertexArrays(1, &floorVao_);
    if (backgroundVao_) gl::DeleteVertexArrays(1, &backgroundVao_);
    meshEbo_ = meshVbo_ = floorVbo_ = 0;
    meshVao_ = floorVao_ = backgroundVao_ = 0;
    backgroundProgram_.destroy();
    floorProgram_.destroy();
    meshProgram_.destroy();
    wireProgram_.destroy();
    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
}


GLuint Renderer::loadTexture(const std::string& path) {
    if (path.empty()) {
        return 0;
    }
    const auto cached = textureCache_.find(path);
    if (cached != textureCache_.end()) {
        return cached->second;
    }

    const std::wstring widePath = widenPath(path);
    if (widePath.empty()) {
        return 0;
    }

    Gdiplus::Bitmap bitmap(widePath.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok || bitmap.GetWidth() == 0 || bitmap.GetHeight() == 0) {
        return 0;
    }

    Gdiplus::Bitmap surface(bitmap.GetWidth(), bitmap.GetHeight(), PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&surface);
    graphics.DrawImage(&bitmap, 0, 0, bitmap.GetWidth(), bitmap.GetHeight());

    Gdiplus::Rect rect(0, 0, bitmap.GetWidth(), bitmap.GetHeight());
    Gdiplus::BitmapData data{};
    if (surface.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) !=
        Gdiplus::Ok) {
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (data.Stride / 4 != static_cast<int>(bitmap.GetWidth())) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, data.Stride / 4);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bitmap.GetWidth(), bitmap.GetHeight(), 0,
                   GL_BGRA, GL_UNSIGNED_BYTE, data.Scan0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl::GenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    surface.UnlockBits(&data);
    textureCache_.emplace(path, texture);
    return texture;
}

void Renderer::buildModelBuffers(const ObjModel& model) {
    for (const GLuint texture : materialTextures_) {
        if (texture) glDeleteTextures(1, &texture);
    }
    materialTextures_.clear();
    textureCache_.clear();

    materialTextures_.assign(model.materials().size(), 0);
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        if (!model.materials()[i].diffuseMap.empty()) {
            materialTextures_[i] = loadTexture(model.materials()[i].diffuseMap);
        }
    }

    if (meshEbo_) gl::DeleteBuffers(1, &meshEbo_);
    if (meshVbo_) gl::DeleteBuffers(1, &meshVbo_);
    if (meshVao_) gl::DeleteVertexArrays(1, &meshVao_);

    gl::GenVertexArrays(1, &meshVao_);
    gl::GenBuffers(1, &meshVbo_);
    gl::GenBuffers(1, &meshEbo_);
    gl::BindVertexArray(meshVao_);

    gl::BindBuffer(GL_ARRAY_BUFFER, meshVbo_);
    gl::BufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(model.vertices().size() * sizeof(MeshVertex)),
                   model.vertices().data(), GL_STATIC_DRAW);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEbo_);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(model.indices().size() * sizeof(std::uint32_t)),
                   model.indices().data(), GL_STATIC_DRAW);

    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                            reinterpret_cast<void*>(offsetof(MeshVertex, position)));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                            reinterpret_cast<void*>(offsetof(MeshVertex, normal)));
    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                            reinterpret_cast<void*>(offsetof(MeshVertex, texcoord)));
    gl::BindVertexArray(0);
}

void Renderer::drawBackground(float aspect) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    gl::UseProgram(backgroundProgram_.id);
    gl::Uniform1f(backgroundProgram_.uniform("uAspect"), aspect);
    gl::BindVertexArray(backgroundVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    gl::BindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawFloor(const Camera& camera, const ModelTransform& transform, const ObjModel& model,
                         float aspect) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl::UseProgram(floorProgram_.id);

    const og::Mat4 view = camera.view();
    const og::Mat4 projection = camera.projection(aspect);
    gl::UniformMatrix4fv(floorProgram_.uniform("uView"), 1, GL_FALSE, view.m);
    gl::UniformMatrix4fv(floorProgram_.uniform("uProjection"), 1, GL_FALSE, projection.m);
    gl::Uniform1f(floorProgram_.uniform("uFloorY"), model.floorY());

    const og::Vec3 cameraPosition = camera.position();
    gl::Uniform3fv(floorProgram_.uniform("uCameraPosition"), 1, &cameraPosition.x);
    const float shadowCenter[2] = {transform.translation.x, transform.translation.z};
    gl::Uniform2fv(floorProgram_.uniform("uShadowCenter"), 1, shadowCenter);
    gl::Uniform1f(floorProgram_.uniform("uShadowRadius"), 1.40f * transform.scale);
    const float shadowStrength = transform.translation.y < -0.02f
        ? 0.0f
        : og::clampf(1.0f - transform.translation.y * 0.22f, 0.18f, 1.0f);
    gl::Uniform1f(floorProgram_.uniform("uShadowStrength"), shadowStrength);

    gl::BindVertexArray(floorVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    gl::BindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawMesh(const ObjModel& model, const Camera& camera, const ModelTransform& transform,
                        float aspect) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    gl::UseProgram(meshProgram_.id);

    const og::Mat4 modelMatrix = transform.matrix();
    const og::Mat4 view = camera.view();
    const og::Mat4 projection = camera.projection(aspect);
    float normal[9];
    og::normalMatrix(modelMatrix, normal);

    gl::UniformMatrix4fv(meshProgram_.uniform("uModel"), 1, GL_FALSE, modelMatrix.m);
    gl::UniformMatrix4fv(meshProgram_.uniform("uView"), 1, GL_FALSE, view.m);
    gl::UniformMatrix4fv(meshProgram_.uniform("uProjection"), 1, GL_FALSE, projection.m);
    gl::UniformMatrix3fv(meshProgram_.uniform("uNormalMatrix"), 1, GL_FALSE, normal);
    const og::Vec3 cameraPosition = camera.position();
    gl::Uniform3fv(meshProgram_.uniform("uCameraPosition"), 1, &cameraPosition.x);

    gl::Uniform1i(meshProgram_.uniform("uDiffuseTexture"), 0);
    gl::ActiveTexture(GL_TEXTURE0);

    auto drawRange = [&](const DrawRange& range) {
        const Material& material = model.materials()[range.material];
        const GLuint texture = range.material < static_cast<int>(materialTextures_.size())
            ? materialTextures_[range.material] : 0;
        const og::Vec3 base = toLinear(
            (!material.diffuseMap.empty() && !material.diffuseSpecified)
                ? og::Vec3(1.0f, 1.0f, 1.0f) : material.diffuse);
        const og::Vec3 specular = toLinear(material.specular);
        gl::Uniform3fv(meshProgram_.uniform("uBaseColor"), 1, &base.x);
        gl::Uniform1i(meshProgram_.uniform("uHasTexture"), texture ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, texture);
        gl::Uniform3fv(meshProgram_.uniform("uSpecularColor"), 1, &specular.x);
        gl::Uniform3fv(meshProgram_.uniform("uEmissiveColor"), 1, &material.emissive.x);
        gl::Uniform1f(meshProgram_.uniform("uShininess"),
                      og::clampf(material.shininess * 0.35f, 4.0f, 220.0f));
        gl::Uniform1f(meshProgram_.uniform("uOpacity"), material.opacity);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(range.indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<void*>(static_cast<std::uintptr_t>(range.firstIndex) *
                                               sizeof(std::uint32_t)));
    };

    gl::BindVertexArray(meshVao_);
    for (const DrawRange& range : model.ranges()) {
        if (model.materials()[range.material].opacity >= 0.999f) {
            drawRange(range);
        }
    }

    bool drewTransparent = false;
    for (const DrawRange& range : model.ranges()) {
        if (model.materials()[range.material].opacity < 0.999f) {
            if (!drewTransparent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                drewTransparent = true;
            }
            drawRange(range);
        }
    }
    if (drewTransparent) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    gl::BindVertexArray(0);
}

void Renderer::drawWireframe(const ObjModel& model, const Camera& camera,
                             const ModelTransform& transform, float aspect) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);

    gl::UseProgram(wireProgram_.id);
    const og::Mat4 modelMatrix = transform.matrix();
    const og::Mat4 view = camera.view();
    const og::Mat4 projection = camera.projection(aspect);
    gl::UniformMatrix4fv(wireProgram_.uniform("uModel"), 1, GL_FALSE, modelMatrix.m);
    gl::UniformMatrix4fv(wireProgram_.uniform("uView"), 1, GL_FALSE, view.m);
    gl::UniformMatrix4fv(wireProgram_.uniform("uProjection"), 1, GL_FALSE, projection.m);
    const float color[3] = {0.18f, 0.78f, 1.0f};
    gl::Uniform3fv(wireProgram_.uniform("uColor"), 1, color);
    gl::Uniform1f(wireProgram_.uniform("uAlpha"), 0.24f);

    gl::BindVertexArray(meshVao_);
    for (const DrawRange& range : model.ranges()) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(range.indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<void*>(static_cast<std::uintptr_t>(range.firstIndex) *
                                               sizeof(std::uint32_t)));
    }
    gl::BindVertexArray(0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
}

void Renderer::draw(const ObjModel& model, const Camera& camera, const ModelTransform& transform,
                    float aspect, bool showGrid, bool showWireframe) {
    drawBackground(aspect);
    drawMesh(model, camera, transform, aspect);
    if (showWireframe) {
        drawWireframe(model, camera, transform, aspect);
    }
    if (showGrid) {
        drawFloor(camera, transform, model, aspect);
    }
}
