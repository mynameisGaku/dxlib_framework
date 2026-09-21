cmake_minimum_required(VERSION 3.24)
project(dxf_render_api_validation LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DXF_RENDER_ASAN "Address and UndefinedBehavior instrumentation" OFF)
option(DXF_RENDER_TSAN "Thread instrumentation in a separate build" OFF)
if(DXF_RENDER_ASAN AND DXF_RENDER_TSAN)
    message(FATAL_ERROR "ASan and TSan require separate builds")
endif()
if(MSVC AND (DXF_RENDER_ASAN OR DXF_RENDER_TSAN))
    message(FATAL_ERROR "This sanitizer configuration requires GCC/Clang on a supported OS")
endif()
find_package(Threads REQUIRED)
add_library(core STATIC
    ${ROOT}/Source/Toolbox/Private/Toolbox/Threading.cpp
    ${ROOT}/Source/Toolbox/Private/Toolbox/JobSystem.cpp
    ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderGeometry3D.cpp
    ${ROOT}/Source/DxLibSupport/Private/Dxf/DebugDrawAdapters.cpp
    ${ROOT}/Source/DxLibSupport/Private/Dxf/Render2DContext.cpp
    ${ROOT}/Source/DxLibSupport/Private/Dxf/Render3DContext.cpp
    ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderQueue2D.cpp
    ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderSystem.cpp)
target_include_directories(core PUBLIC ${ROOT}/Source/Toolbox/Public ${ROOT}/Source/Foundation/Public ${ROOT}/Source/DxLibSupport/Public)
target_compile_features(core PUBLIC cxx_std_20)
target_link_libraries(core PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(core PUBLIC /W4 /WX /permissive- /utf-8 /EHsc /GR)
else()
    target_compile_options(core PUBLIC -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    if(DXF_RENDER_ASAN)
        target_compile_options(core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(core PUBLIC -fsanitize=address,undefined)
    elseif(DXF_RENDER_TSAN)
        target_compile_options(core PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(core PUBLIC -fsanitize=thread)
    endif()
endif()
enable_testing()
include(${ROOT}/CMake/RenderContinuationTests.cmake)
dxf_add_render_continuation_tests(core)
include(${ROOT}/CMake/RenderViewsTests.cmake)
dxf_add_render_views_tests(core)
