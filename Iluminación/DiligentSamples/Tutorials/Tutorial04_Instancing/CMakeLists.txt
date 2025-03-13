cmake_minimum_required (VERSION 3.10)

project(Tutorial04_Instancing CXX)

set(SOURCE
    src/Tutorial04_Instancing.cpp
    ../Common/src/TexturedCube.cpp
    src/Sphere.cpp
)

set(INCLUDE
    src/Tutorial04_Instancing.hpp
    ../Common/src/TexturedCube.hpp
    src/Sphere.hpp
)

set(SHADERS
    assets/cube_inst.vsh
    assets/cube_inst.psh
    assets/sphere_inst.psh
    assets/sphere_inst.vsh
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("Tutorial04_Instancing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
