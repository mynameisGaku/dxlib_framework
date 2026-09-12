cmake_minimum_required(VERSION 3.24)
project(dxlib_physics_validation LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(DXF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(DXF_PHYSICS_SANITIZERS "Enable ASan and UBSan on GCC/Clang" OFF)
add_library(dxf_physics_core STATIC
    "${DXF_ROOT}/Source/Toolbox/Private/Vector3.cpp"
    "${DXF_ROOT}/Source/Toolbox/Private/CollisionShapes.cpp"
    "${DXF_ROOT}/Source/Toolbox/Private/Mesh.cpp"
    "${DXF_ROOT}/Source/Toolbox/Private/Collision2D.cpp"
    "${DXF_ROOT}/Source/Toolbox/Private/ContinuousCollision.cpp"
    "${DXF_ROOT}/Source/Toolbox/Private/FixedStepScheduler.cpp")
target_include_directories(dxf_physics_core PUBLIC "${DXF_ROOT}/Source/Toolbox/Public")
target_compile_features(dxf_physics_core PUBLIC cxx_std_20)
enable_testing()
include("${DXF_ROOT}/CMake/PhysicsTests.cmake")
dxf_add_physics_tests(dxf_physics_tests dxf_physics_core)
foreach(_target dxf_physics_core dxf_physics_tests)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /GR)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    endif()
endforeach()
if(DXF_PHYSICS_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "The combined address/undefined profile requires GCC or Clang")
    endif()
    foreach(_target dxf_physics_core dxf_physics_tests)
        target_compile_options(${_target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endforeach()
    target_link_options(dxf_physics_tests PRIVATE -fsanitize=address,undefined)
endif()
