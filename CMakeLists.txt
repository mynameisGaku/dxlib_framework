cmake_minimum_required(VERSION 3.24)
project(dxlib_framework VERSION 0.3.0 LANGUAGES CXX)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

# Windows検証スクリプトが実際のコンパイラ出力から取得した依存ファイル行の接頭辞。
set(DXF_MSVC_INCLUDE_PREFIX "" CACHE STRING "Measured MSVC showIncludes prefix; empty uses CMake detection")
if(MSVC AND DXF_MSVC_INCLUDE_PREFIX)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${DXF_MSVC_INCLUDE_PREFIX}")
endif()

option(DXF_BUILD_TESTS "Build portable and native-contract regression tests" ${PROJECT_IS_TOP_LEVEL})
option(DXF_BUILD_NATIVE "Build the Windows/MSVC DxLib adapter" OFF)
option(DXF_BUILD_NATIVE_SMOKE "Build the self-terminating real-SDK smoke executable" ${DXF_BUILD_NATIVE})
option(DXF_RUN_DEVICE_TESTS "Register smoke tests requiring a Windows desktop and devices" OFF)
option(DXF_BUILD_EXAMPLE "Build the Windows Sandbox" OFF)
option(DXF_BUILD_STARTER "Build the empty Windows starter project" ${DXF_BUILD_EXAMPLE})
option(DXF_INSTALL "Generate installation and package-export rules" ${PROJECT_IS_TOP_LEVEL})
option(DXF_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(DXF_COVERAGE "Enable GCC source coverage instrumentation" OFF)
option(DXF_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
set(DXLIB_ROOT "" CACHE PATH "Official VC SDK root or directory containing DxLib.h and libraries")
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC AND DXF_BUILD_NATIVE)
    # The official VC SDK uses the static CRT. This also covers our examples and tests.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(dxf_warnings Target)
    if(MSVC)
        target_compile_options(${Target} PRIVATE /W4 /permissive- /utf-8 /EHsc /GR)
        if(DXF_WARNINGS_AS_ERRORS)
            target_compile_options(${Target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(DXF_WARNINGS_AS_ERRORS)
            target_compile_options(${Target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(dxf_ide_headers Target)
    foreach(Directory IN LISTS ARGN)
        file(GLOB_RECURSE Headers CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${Directory}/*.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/${Directory}/*.hpp")
        if(Headers)
            # PRIVATE also gives header-only INTERFACE libraries an IDE project,
            # without exporting source-tree paths to package consumers.
            target_sources(${Target} PRIVATE ${Headers})
        endif()
    endforeach()
    # cppとヘッダーを、リポジトリからの実際の相対パスで表示する。
    get_target_property(Sources ${Target} SOURCES)
    if(Sources)
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
    endif()
endfunction()

function(dxf_public_headers Target Layer Visibility)
    target_include_directories(${Target} ${Visibility}
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/${Layer}/Public>"
        "$<INSTALL_INTERFACE:include>")
    dxf_ide_headers(${Target} "Source/${Layer}")
endfunction()

add_library(dxf_toolbox STATIC
    Source/Toolbox/Private/Platform.cpp
    Source/Toolbox/Private/Vector3.cpp
    Source/Toolbox/Private/Matrix4.cpp
    Source/Toolbox/Private/Quaternion.cpp
    Source/Toolbox/Private/Complex.cpp
    Source/Toolbox/Private/CollisionShapes.cpp
    Source/Toolbox/Private/Collision2D.cpp
    Source/Toolbox/Private/ContinuousCollision.cpp
    Source/Toolbox/Private/FixedStepScheduler.cpp
    Source/Toolbox/Private/Mesh.cpp
    Source/Toolbox/Private/CollisionWorld.cpp)
add_library(dxf::toolbox ALIAS dxf_toolbox)
set_target_properties(dxf_toolbox PROPERTIES EXPORT_NAME toolbox DEBUG_POSTFIX d)
target_compile_features(dxf_toolbox PUBLIC cxx_std_20)
target_include_directories(dxf_toolbox PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/Toolbox/Public>" "$<INSTALL_INTERFACE:include>")
dxf_ide_headers(dxf_toolbox Source/Toolbox)
dxf_warnings(dxf_toolbox)

add_library(dxf_foundation INTERFACE)
target_link_libraries(dxf_foundation INTERFACE dxf::toolbox)
add_library(dxf::foundation ALIAS dxf_foundation)
set_target_properties(dxf_foundation PROPERTIES EXPORT_NAME foundation)
target_compile_features(dxf_foundation INTERFACE cxx_std_20)
dxf_public_headers(dxf_foundation Foundation INTERFACE)
if(DXF_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "The address+undefined sanitizer profile requires GCC or Clang on Linux")
    endif()
    target_compile_options(dxf_foundation INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(dxf_toolbox PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(dxf_foundation INTERFACE -fsanitize=address,undefined)
endif()
if(DXF_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "DXF_COVERAGE requires GCC/gcov")
    endif()
    target_compile_options(dxf_foundation INTERFACE --coverage)
    target_compile_options(dxf_toolbox PRIVATE --coverage)
    target_link_options(dxf_foundation INTERFACE --coverage)
endif()

add_library(dxf_support STATIC
    Source/DxLibSupport/Private/AssetService.cpp
    Source/DxLibSupport/Private/TextureLoader.cpp
    Source/DxLibSupport/Private/SoundLoader.cpp
    Source/DxLibSupport/Private/FontLoader.cpp
    Source/DxLibSupport/Private/RenderSystem2D.cpp
    Source/DxLibSupport/Private/RenderQueue2D.cpp
    Source/DxLibSupport/Private/AudioPlayer.cpp)
add_library(dxf::support ALIAS dxf_support)
set_target_properties(dxf_support PROPERTIES EXPORT_NAME support DEBUG_POSTFIX d)
target_link_libraries(dxf_support PUBLIC dxf::foundation)
dxf_public_headers(dxf_support DxLibSupport PUBLIC)
dxf_warnings(dxf_support)

add_library(dxf_runtime STATIC
    Source/Runtime/Private/LifecycleObject.cpp
    Source/Runtime/Private/SceneNavigator.cpp
    Source/Runtime/Private/Application.cpp)
add_library(dxf::runtime ALIAS dxf_runtime)
set_target_properties(dxf_runtime PROPERTIES EXPORT_NAME runtime DEBUG_POSTFIX d)
target_link_libraries(dxf_runtime PUBLIC dxf::support)
dxf_public_headers(dxf_runtime Runtime PUBLIC)
dxf_warnings(dxf_runtime)

add_library(dxf_gameplay INTERFACE)
add_library(dxf::gameplay ALIAS dxf_gameplay)
set_target_properties(dxf_gameplay PROPERTIES EXPORT_NAME gameplay)
target_link_libraries(dxf_gameplay INTERFACE dxf::runtime)
dxf_public_headers(dxf_gameplay Gameplay INTERFACE)

add_library(dxf_physics STATIC
    Source/Physics/Private/PhysicsWorld2D.cpp
    Source/Physics/Private/PhysicsWorld3D.cpp)
add_library(dxf::physics ALIAS dxf_physics)
set_target_properties(dxf_physics PROPERTIES EXPORT_NAME physics DEBUG_POSTFIX d)
target_compile_features(dxf_physics PUBLIC cxx_std_20)
target_link_libraries(dxf_physics PUBLIC dxf::foundation)
dxf_public_headers(dxf_physics Physics PUBLIC)
dxf_warnings(dxf_physics)

add_library(dxlib_framework INTERFACE)
add_library(dxf::framework ALIAS dxlib_framework)
set_target_properties(dxlib_framework PROPERTIES EXPORT_NAME framework)
target_link_libraries(dxlib_framework INTERFACE dxf::gameplay)

set(DXF_NATIVE_SOURCES
    Source/Native/Private/DxLibPlatform.cpp
    Source/Native/Private/DxLibInputSource.cpp
    Source/Native/Private/DxLibTextureBackend.cpp
    Source/Native/Private/DxLibSoundBackend.cpp
    Source/Native/Private/DxLibFontBackend.cpp
    Source/Native/Private/DxLibRenderBackend.cpp)

if(DXF_BUILD_TESTS)
    enable_testing()
    include(CMake/PhysicsTests.cmake)
    dxf_add_physics_tests(dxf_physics_tests dxf::physics)
    dxf_warnings(dxf_physics_tests)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME NoStl COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/CheckNoStl.py")
        set_tests_properties(NoStl PROPERTIES LABELS "source-policy" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    add_executable(dxf_tests Tests/TestMain.cpp Tests/ToolboxTests.cpp Tests/MathCollisionTests.cpp Tests/FoundationTests.cpp Tests/SupportTests.cpp
        Tests/GameplayTests.cpp Tests/ApplicationTests.cpp Tests/SandboxTests.cpp
        Tests/HardeningTests.cpp Tests/RenderFailureTests.cpp Tests/SceneContinuationTests.cpp Tests/ResourceShutdownTests.cpp Tests/UsabilityTests.cpp Tests/StressTests.cpp Tests/StarterTests.cpp Examples/Starter/Source/BootScene.cpp Examples/Sandbox/SandboxGame.cpp)
    target_link_libraries(dxf_tests PRIVATE dxf::framework)
    target_include_directories(dxf_tests PRIVATE Tests Examples/Sandbox Examples/Starter/Source)
    dxf_ide_headers(dxf_tests Tests/Support Examples/Sandbox Examples/Starter/Source)
    dxf_warnings(dxf_tests)
    add_test(NAME Framework COMMAND dxf_tests)
    set_tests_properties(Framework PROPERTIES TIMEOUT 60 LABELS "portable")

    add_executable(dxf_native_contract_tests Tests/TestMain.cpp Tests/NativeTests.cpp Tests/NativeMacroTests.cpp Tests/NativeSmoke/Smoke.cpp ${DXF_NATIVE_SOURCES})
    target_link_libraries(dxf_native_contract_tests PRIVATE dxf::framework)
    target_include_directories(dxf_native_contract_tests PRIVATE Tests Tests/FakeDxLib Source/Native/Public)
    dxf_ide_headers(dxf_native_contract_tests Tests/Support Tests/FakeDxLib Tests/NativeSmoke Source/Native)
    target_compile_definitions(dxf_native_contract_tests PRIVATE DXF_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets")
    dxf_warnings(dxf_native_contract_tests)
    add_test(NAME NativeContract COMMAND dxf_native_contract_tests)
    set_tests_properties(NativeContract PROPERTIES TIMEOUT 60 LABELS "contract;not-real-sdk")
endif()

set(DXF_EXPORT_TARGETS dxlib_framework dxf_toolbox dxf_foundation dxf_support dxf_runtime dxf_gameplay dxf_physics)
if(DXF_BUILD_NATIVE)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "DXF_BUILD_NATIVE requires Windows/MSVC x64. Portable tests do not need the SDK.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Use an x64 Visual Studio developer environment")
    endif()
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
    find_package(DxLib REQUIRED)
    add_library(dxf_native_backends STATIC ${DXF_NATIVE_SOURCES})
    add_library(dxf::native_backends ALIAS dxf_native_backends)
    set_target_properties(dxf_native_backends PROPERTIES EXPORT_NAME native_backends DEBUG_POSTFIX d)
    target_link_libraries(dxf_native_backends PUBLIC dxf::support DxLib::SDK)
    dxf_public_headers(dxf_native_backends Native PUBLIC)
    target_compile_definitions(dxf_native_backends PRIVATE DX_NON_USING_NAMESPACE_DXLIB NOMINMAX)
    target_compile_options(dxf_native_backends PRIVATE /UUNICODE /U_UNICODE)
    dxf_warnings(dxf_native_backends)

    add_library(dxf_native INTERFACE)
    add_library(dxf::native ALIAS dxf_native)
    set_target_properties(dxf_native PROPERTIES EXPORT_NAME native)
    target_link_libraries(dxf_native INTERFACE dxf::native_backends dxf::framework)
    list(APPEND DXF_EXPORT_TARGETS dxf_native_backends dxf_native)
endif()

if(DXF_BUILD_NATIVE_SMOKE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_NATIVE_SMOKE requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(NativeSmoke Tests/NativeSmoke/Main.cpp Tests/NativeSmoke/Smoke.cpp)
    dxf_ide_headers(NativeSmoke Tests/NativeSmoke)
    target_link_libraries(NativeSmoke PRIVATE dxf::native_backends)
    dxf_warnings(NativeSmoke)
    if(DXF_RUN_DEVICE_TESTS)
        enable_testing()
        add_test(NAME NativeDeviceSmoke COMMAND NativeSmoke)
        set_tests_properties(NativeDeviceSmoke PROPERTIES TIMEOUT 60 LABELS "real-sdk;device"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:NativeSmoke>")
    endif()
elseif(DXF_RUN_DEVICE_TESTS)
    message(FATAL_ERROR "DXF_RUN_DEVICE_TESTS requires DXF_BUILD_NATIVE_SMOKE=ON")
endif()

if(DXF_BUILD_EXAMPLE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_EXAMPLE requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(Sandbox WIN32 Examples/Sandbox/WindowsMain.cpp Examples/Sandbox/SandboxGame.cpp)
    dxf_ide_headers(Sandbox Examples/Sandbox)
    target_link_libraries(Sandbox PRIVATE dxf::native)
    dxf_warnings(Sandbox)
    set_property(TARGET Sandbox PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Sandbox>")
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Sandbox)
endif()

if(DXF_BUILD_STARTER)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_STARTER requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(Starter WIN32 Examples/Starter/Source/WindowsMain.cpp Examples/Starter/Source/BootScene.cpp)
    dxf_ide_headers(Starter Examples/Starter/Source)
    target_link_libraries(Starter PRIVATE dxf::native)
    dxf_warnings(Starter)
    set_property(TARGET Starter PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Starter>")
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Starter)
endif()

# One writer for shared assets; parallel links must not copy into the same destination.
if(TARGET NativeSmoke OR TARGET Sandbox)
    if(TARGET Sandbox)
        set(_dxf_asset_target Sandbox)
    else()
        set(_dxf_asset_target NativeSmoke)
    endif()
    add_custom_target(dxf_sample_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
            "$<TARGET_FILE_DIR:${_dxf_asset_target}>/Assets"
        COMMENT "Copying sample assets")
    foreach(_dxf_executable NativeSmoke Sandbox)
        if(TARGET ${_dxf_executable})
            add_dependencies(${_dxf_executable} dxf_sample_assets)
        endif()
    endforeach()
endif()

if(DXF_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS ${DXF_EXPORT_TARGETS} EXPORT dxlib_frameworkTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    foreach(Layer Toolbox Foundation DxLibSupport Runtime Gameplay Physics Native)
        install(DIRECTORY "Source/${Layer}/Public/" DESTINATION include FILES_MATCHING PATTERN "*.h")
    endforeach()
    set(DXF_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dxlib_framework")
    configure_package_config_file(CMake/dxlib_frameworkConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfig.cmake"
        INSTALL_DESTINATION "${DXF_CONFIG_DESTINATION}")
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfigVersion.cmake"
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
    install(EXPORT dxlib_frameworkTargets NAMESPACE dxf:: DESTINATION "${DXF_CONFIG_DESTINATION}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfigVersion.cmake" CMake/FindDxLib.cmake
        DESTINATION "${DXF_CONFIG_DESTINATION}")
    install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/dxlib_framework")
endif()
