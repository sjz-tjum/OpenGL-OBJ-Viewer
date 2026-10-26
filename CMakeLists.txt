cmake_minimum_required(VERSION 3.16)
project(OpenGL_OBJ_Viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(OpenGL_OBJ_Viewer WIN32
    src/main.cpp
    src/GLFunctions.cpp
    src/ObjModel.cpp
    src/Renderer.cpp
    src/TextRenderer.cpp
)

target_include_directories(OpenGL_OBJ_Viewer PRIVATE src)
target_link_libraries(OpenGL_OBJ_Viewer PRIVATE opengl32 gdi32 user32 shell32 comdlg32 gdiplus)

if(MINGW)
    target_link_options(OpenGL_OBJ_Viewer PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(OpenGL_OBJ_Viewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
