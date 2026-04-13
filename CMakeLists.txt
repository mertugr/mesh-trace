cmake_minimum_required(VERSION 3.16)
project(TriangleRayTracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(raytracer
    src/main.cpp
    src/core/BVH.cpp
    src/io/Image.cpp
    src/io/SceneParser.cpp
    src/render/Renderer.cpp
    src/scene/Camera.cpp
    src/scene/Scene.cpp
    src/scene/Texture.cpp
    src/scene/Triangle.cpp
    tinyxml2.cpp
)

target_include_directories(raytracer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(raytracer PRIVATE Threads::Threads)

target_compile_options(raytracer PRIVATE -Wall -Wextra -O2)
