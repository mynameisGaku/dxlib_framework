cmake_minimum_required(VERSION 3.24)
project(DxfSceneTaskIntegration LANGUAGES CXX)
get_filename_component(_default_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(DXF_SCENE_SOURCE_ROOT "${_default_root}" CACHE PATH "Framework source root (allows comparison with an unchanged baseline)")
option(DXF_SCENE_FOCUSED "Compile only the real Application dependency subset, not the full framework" OFF)
option(DXF_SCENE_SANITIZERS "Use address and undefined sanitizers (GCC/Clang)" OFF)
option(DXF_SCENE_THREAD_SANITIZER "Use thread sanitizer (GCC/Clang)" OFF)
if(DXF_SCENE_SANITIZERS AND DXF_SCENE_THREAD_SANITIZER)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer require separate build directories")
endif()
if(MSVC AND (DXF_SCENE_SANITIZERS OR DXF_SCENE_THREAD_SANITIZER))
    message(FATAL_ERROR "These sanitizer profiles require GCC or Clang on Linux")
endif()
if(DXF_SCENE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(DXF_SCENE_THREAD_SANITIZER)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
enable_testing()
if(DXF_SCENE_FOCUSED)
    # 外部Backend以外のApplication依存は、指定Rootの実装をそのままコンパイルする。
    # この経路の成功をPhysics/Nativeを含む全体ビルドとは数えない。
    find_package(Threads REQUIRED)
    set(_sources
        Source/Runtime/Private/Dxf/Application.cpp
        Source/Runtime/Private/Dxf/SceneNavigator.cpp
        Source/Runtime/Private/Dxf/LifecycleObject.cpp
        Source/Runtime/Private/Dxf/TaskDispatcher.cpp
        Source/Toolbox/Private/Toolbox/Threading.cpp
        Source/Toolbox/Private/Toolbox/JobSystem.cpp
        Source/Toolbox/Private/Toolbox/Log.cpp
        Source/Toolbox/Private/Toolbox/Platform.cpp
        Source/Toolbox/Private/Toolbox/ProjectPaths.cpp
        Source/Toolbox/Private/Toolbox/Vector3.cpp
        Source/DxLibSupport/Private/Dxf/AssetService.cpp
        Source/DxLibSupport/Private/Dxf/AudioPlayer.cpp
        Source/DxLibSupport/Private/Dxf/TextureLoader.cpp
        Source/DxLibSupport/Private/Dxf/SoundLoader.cpp
        Source/DxLibSupport/Private/Dxf/FontLoader.cpp
        Source/DxLibSupport/Private/Dxf/RenderQueue2D.cpp
        Source/DxLibSupport/Private/Dxf/Render2DContext.cpp
        Source/DxLibSupport/Private/Dxf/Render3DContext.cpp
        Source/DxLibSupport/Private/Dxf/RenderGeometry3D.cpp)
    # ローカルに適用済みのRenderer統合を逆戻りさせない。
    file(READ "${DXF_SCENE_SOURCE_ROOT}/Source/Runtime/Public/Dxf/Application.h" _application_header)
    if(_application_header MATCHES "Dxf/RenderSystem2D.h")
        list(APPEND _sources Source/DxLibSupport/Private/Dxf/RenderSystem2D.cpp)
    elseif(_application_header MATCHES "Dxf/RenderSystem.h")
        list(APPEND _sources Source/DxLibSupport/Private/Dxf/RenderSystem.cpp
            Source/DxLibSupport/Private/Dxf/RenderPass3D.cpp)
    else()
        message(FATAL_ERROR "Unknown Application renderer; do not guess a replacement")
    endif()
    list(TRANSFORM _sources PREPEND "${DXF_SCENE_SOURCE_ROOT}/")
    add_library(dxf_scene_task_runtime STATIC ${_sources})
    target_link_libraries(dxf_scene_task_runtime PUBLIC Threads::Threads)
    target_compile_features(dxf_scene_task_runtime PUBLIC cxx_std_20)
    foreach(_layer Runtime Toolbox Foundation DxLibSupport)
        target_include_directories(dxf_scene_task_runtime PUBLIC "${DXF_SCENE_SOURCE_ROOT}/Source/${_layer}/Public")
    endforeach()
    if(MSVC)
        target_compile_options(dxf_scene_task_runtime PRIVATE /W4 /utf-8 /permissive- /EHsc)
        set(_strict /WX)
    else()
        target_compile_options(dxf_scene_task_runtime PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        set(_strict -Werror)
    endif()
    # 既存依存の警告は保持し、新規変更した実装には警告エラー扱いを適用する。
    set_property(SOURCE
        "${DXF_SCENE_SOURCE_ROOT}/Source/Runtime/Private/Dxf/Application.cpp"
        "${DXF_SCENE_SOURCE_ROOT}/Source/Runtime/Private/Dxf/SceneNavigator.cpp"
        "${DXF_SCENE_SOURCE_ROOT}/Source/Runtime/Private/Dxf/TaskDispatcher.cpp"
        APPEND PROPERTY COMPILE_OPTIONS ${_strict})
    set(_runtime dxf_scene_task_runtime)
else()
    # 通常はリポジトリ本体のターゲットへリンクする。旧テストをこの専用構成で再登録しない。
    set(DXF_BUILD_TESTS OFF CACHE BOOL "Enable unrelated framework tests" )
    set(DXF_BUILD_NATIVE OFF CACHE BOOL "Enable real DxLib adapter" )
    set(DXF_BUILD_NATIVE_SMOKE OFF CACHE BOOL "Enable real SDK smoke" )
    set(DXF_BUILD_EXAMPLE OFF CACHE BOOL "Enable example" )
    set(DXF_BUILD_STARTER OFF CACHE BOOL "Enable starter" )
    set(DXF_INSTALL OFF CACHE BOOL "Enable installation" )
    add_subdirectory("${DXF_SCENE_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/framework")
    set(_runtime dxf::framework)
endif()
include("${_default_root}/CMake/SceneTaskIntegrationTests.cmake")
dxf_add_scene_task_integration_tests(dxf_scene_task_integration_tests ${_runtime})
