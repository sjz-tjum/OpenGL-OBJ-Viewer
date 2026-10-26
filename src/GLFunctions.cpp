#include "GLFunctions.h"

#include <string>

namespace gl {

PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
PFNGLGENBUFFERSPROC GenBuffers = nullptr;
PFNGLBINDBUFFERPROC BindBuffer = nullptr;
PFNGLBUFFERDATAPROC BufferData = nullptr;
PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
PFNGLCREATESHADERPROC CreateShader = nullptr;
PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
PFNGLCOMPILESHADERPROC CompileShader = nullptr;
PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC DeleteShader = nullptr;
PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
PFNGLATTACHSHADERPROC AttachShader = nullptr;
PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC UseProgram = nullptr;
PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC Uniform1i = nullptr;
PFNGLUNIFORM1FPROC Uniform1f = nullptr;
PFNGLUNIFORM2FVPROC Uniform2fv = nullptr;
PFNGLUNIFORM3FVPROC Uniform3fv = nullptr;
PFNGLUNIFORM4FVPROC Uniform4fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv = nullptr;
PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv = nullptr;
PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;

namespace {

std::string g_error;
HMODULE g_opengl32 = nullptr;

void* resolve(const char* name) {
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc == nullptr || proc == reinterpret_cast<void*>(1) ||
        proc == reinterpret_cast<void*>(2) || proc == reinterpret_cast<void*>(3) ||
        proc == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
        proc = reinterpret_cast<void*>(GetProcAddress(g_opengl32, name));
    }
    return proc;
}

template <typename T>
bool assign(T& target, const char* name) {
    target = reinterpret_cast<T>(resolve(name));
    if (!target) {
        g_error = std::string("Missing OpenGL entry point: ") + name;
        return false;
    }
    return true;
}

} // namespace

bool loadFunctions() {
    g_opengl32 = GetModuleHandleA("opengl32.dll");
    if (!g_opengl32) {
        g_error = "opengl32.dll is not loaded.";
        return false;
    }

    return assign(GenVertexArrays, "glGenVertexArrays")
        && assign(BindVertexArray, "glBindVertexArray")
        && assign(DeleteVertexArrays, "glDeleteVertexArrays")
        && assign(GenBuffers, "glGenBuffers")
        && assign(BindBuffer, "glBindBuffer")
        && assign(BufferData, "glBufferData")
        && assign(DeleteBuffers, "glDeleteBuffers")
        && assign(VertexAttribPointer, "glVertexAttribPointer")
        && assign(EnableVertexAttribArray, "glEnableVertexAttribArray")
        && assign(CreateShader, "glCreateShader")
        && assign(ShaderSource, "glShaderSource")
        && assign(CompileShader, "glCompileShader")
        && assign(GetShaderiv, "glGetShaderiv")
        && assign(GetShaderInfoLog, "glGetShaderInfoLog")
        && assign(DeleteShader, "glDeleteShader")
        && assign(CreateProgram, "glCreateProgram")
        && assign(AttachShader, "glAttachShader")
        && assign(LinkProgram, "glLinkProgram")
        && assign(GetProgramiv, "glGetProgramiv")
        && assign(GetProgramInfoLog, "glGetProgramInfoLog")
        && assign(UseProgram, "glUseProgram")
        && assign(DeleteProgram, "glDeleteProgram")
        && assign(GetUniformLocation, "glGetUniformLocation")
        && assign(Uniform1i, "glUniform1i")
        && assign(Uniform1f, "glUniform1f")
        && assign(Uniform2fv, "glUniform2fv")
        && assign(Uniform3fv, "glUniform3fv")
        && assign(Uniform4fv, "glUniform4fv")
        && assign(UniformMatrix4fv, "glUniformMatrix4fv")
        && assign(UniformMatrix3fv, "glUniformMatrix3fv")
        && assign(ActiveTexture, "glActiveTexture")
        && assign(GenerateMipmap, "glGenerateMipmap");
}

const char* lastError() {
    return g_error.c_str();
}

} // namespace gl
