cmake_minimum_required(VERSION 3.24)
project(dxlib_framework VERSION 0.1.0 LANGUAGES CXX)
option(DXF_BUILD_TESTS "Build portable regression tests" ${PROJECT_IS_TOP_LEVEL})
option(DXF_BUILD_NATIVE "Build the Windows DxLib adapter" OFF)
option(DXF_BUILD_EXAMPLE "Build the native Sandbox" OFF)
option(DXF_SANITIZERS "Enable address/undefined sanitizers" OFF)
set(DXLIB_ROOT "" CACHE PATH "Directory containing DxLib.h and the matching libraries")
if(MSVC AND DXF_BUILD_NATIVE)
    # Match the official VC package: /MTd in Debug, /MT otherwise.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
add_library(dxlib_framework STATIC
    Source/DxLibSupport/Private/AssetService.cpp
    Source/DxLibSupport/Private/TextureLoader.cpp
    Source/DxLibSupport/Private/SoundLoader.cpp
    Source/DxLibSupport/Private/FontLoader.cpp
    Source/DxLibSupport/Private/RenderSystem2D.cpp
    Source/DxLibSupport/Private/RenderQueue2D.cpp
    Source/DxLibSupport/Private/AudioPlayer.cpp
    Source/Runtime/Private/LifecycleObject.cpp
    Source/Runtime/Private/SceneNavigator.cpp
    Source/Runtime/Private/Application.cpp)
add_library(dxf::framework ALIAS dxlib_framework)
target_compile_features(dxlib_framework PUBLIC cxx_std_20)
foreach(Layer Foundation DxLibSupport Runtime Gameplay)
    target_include_directories(dxlib_framework PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/${Layer}/Public>")
endforeach()
function(dxf_warnings Target)
    if(MSVC)
        target_compile_options(${Target} PRIVATE /W4 /permissive- /utf-8 /EHsc /GR)
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()
dxf_warnings(dxlib_framework)
if(DXF_SANITIZERS AND NOT MSVC)
    target_compile_options(dxlib_framework PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(dxlib_framework PUBLIC -fsanitize=address,undefined)
endif()
set(DXF_NATIVE_SOURCES
    Source/Native/Private/DxLibPlatform.cpp
    Source/Native/Private/DxLibInputSource.cpp
    Source/Native/Private/DxLibTextureBackend.cpp
    Source/Native/Private/DxLibSoundBackend.cpp
    Source/Native/Private/DxLibFontBackend.cpp
    Source/Native/Private/DxLibRenderBackend.cpp
)
if(DXF_BUILD_TESTS)
    enable_testing()
    add_executable(dxf_tests Tests/TestMain.cpp Tests/FoundationTests.cpp Tests/SupportTests.cpp Tests/GameplayTests.cpp Tests/ApplicationTests.cpp Tests/SandboxTests.cpp Examples/Sandbox/SandboxGame.cpp)
    target_link_libraries(dxf_tests PRIVATE dxf::framework)
    target_include_directories(dxf_tests PRIVATE Tests Examples/Sandbox)
    dxf_warnings(dxf_tests)
    add_test(NAME Framework COMMAND dxf_tests)
    add_executable(dxf_native_contract_tests Tests/TestMain.cpp Tests/NativeTests.cpp ${DXF_NATIVE_SOURCES})
    target_link_libraries(dxf_native_contract_tests PRIVATE dxf::framework)
    target_include_directories(dxf_native_contract_tests PRIVATE Tests Tests/FakeDxLib Source/Native/Public)
    dxf_warnings(dxf_native_contract_tests)
    add_test(NAME NativeContract COMMAND dxf_native_contract_tests)
endif()

if(DXF_BUILD_NATIVE)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "The native adapter currently supports Windows/MSVC only. Leave DXF_BUILD_NATIVE=OFF for portable tests.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "This configuration targets x64. Use an x64 Visual Studio developer shell.")
    endif()
    find_path(DXF_DXLIB_INCLUDE_DIR DxLib.h
        HINTS "${DXLIB_ROOT}"
        PATH_SUFFIXES "" "プロジェクトに追加すべきファイル_VC用"
        NO_DEFAULT_PATH)
    if(NOT DXF_DXLIB_INCLUDE_DIR)
        message(FATAL_ERROR "DxLib.h was not found. Set DXLIB_ROOT to the official VC SDK include/library directory.")
    endif()
    file(GLOB DXF_SDK_LIBRARIES "${DXF_DXLIB_INCLUDE_DIR}/*.lib")
    if(NOT DXF_SDK_LIBRARIES)
        message(FATAL_ERROR "DxLib .lib files are required beside DxLib.h. A header-only SDK copy is insufficient.")
    endif()
    add_library(dxf_native STATIC ${DXF_NATIVE_SOURCES})
    add_library(dxf::native ALIAS dxf_native)
    target_link_libraries(dxf_native PUBLIC dxf::framework)
    target_include_directories(dxf_native PUBLIC Source/Native/Public)
    target_include_directories(dxf_native SYSTEM PRIVATE "${DXF_DXLIB_INCLUDE_DIR}")
    # DxLib.h selects the matching SDK libraries through its MSVC autolink directives.
    target_link_directories(dxf_native PUBLIC "${DXF_DXLIB_INCLUDE_DIR}")
    target_compile_definitions(dxf_native PRIVATE DX_NON_USING_NAMESPACE_DXLIB NOMINMAX)
    target_compile_options(dxf_native PRIVATE /UUNICODE /U_UNICODE)
    dxf_warnings(dxf_native)
endif()
if(DXF_BUILD_EXAMPLE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_EXAMPLE requires DXF_BUILD_NATIVE=ON and the real Windows SDK.")
    endif()
    add_executable(Sandbox WIN32 Examples/Sandbox/WindowsMain.cpp Examples/Sandbox/SandboxGame.cpp)
    target_link_libraries(Sandbox PRIVATE dxf::native)
    dxf_warnings(Sandbox)
    set_property(TARGET Sandbox PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Sandbox>")
    add_custom_command(TARGET Sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:Sandbox>/Assets")
endif()
