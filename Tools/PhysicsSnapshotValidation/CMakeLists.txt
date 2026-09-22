cmake_minimum_required(VERSION 3.24)
project(DxfPhysicsSnapshotValidation LANGUAGES CXX)
get_filename_component(DXF_SNAPSHOT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(DXF_SNAPSHOT_CORE_ONLY "Build shared-capture tests without a full World; not World integration" OFF)
option(DXF_SNAPSHOT_SANITIZERS "Enable Clang/GCC address+undefined sanitizers" OFF)

if(NOT DXF_SNAPSHOT_CORE_ONLY)
    # Complete source tree, actual Physics and Toolbox. No validation stubs.
    set(DXF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(DXF_BUILD_NATIVE OFF CACHE BOOL "" FORCE)
    set(DXF_BUILD_NATIVE_SMOKE OFF CACHE BOOL "" FORCE)
    set(DXF_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(DXF_BUILD_STARTER OFF CACHE BOOL "" FORCE)
    set(DXF_INSTALL OFF CACHE BOOL "" FORCE)
    set(DXF_SANITIZERS ${DXF_SNAPSHOT_SANITIZERS} CACHE BOOL "" FORCE)
    add_subdirectory("${DXF_SNAPSHOT_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/framework" EXCLUDE_FROM_ALL)
endif()

enable_testing()
function(dxf_snapshot_tests Target Source Prefix)
    add_executable(${Target} Main.cpp "${DXF_SNAPSHOT_ROOT}/Tests/${Source}")
    target_compile_features(${Target} PRIVATE cxx_std_20)
    set_target_properties(${Target} PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(${Target} PRIVATE
        "${DXF_SNAPSHOT_ROOT}/Tests"
        "${DXF_SNAPSHOT_ROOT}/Source/Physics/Public"
        "${DXF_SNAPSHOT_ROOT}/Source/Physics/Private/Dxf"
        "${DXF_SNAPSHOT_ROOT}/Source/Toolbox/Public")
    if(MSVC)
        # C4324 reports the intentional alignas(16) padding of FVector3/FQuaternion in
        # existing Physics headers. Only that note is excluded; other warnings stay errors.
        target_compile_options(${Target} PRIVATE /W4 /WX /wd4324 /utf-8 /permissive- /EHsc)
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
        if(DXF_SNAPSHOT_SANITIZERS)
            target_compile_options(${Target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${Target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    file(STRINGS "${DXF_SNAPSHOT_ROOT}/Tests/${Source}" Cases REGEX "^TEST\\(\"${Prefix}")
    foreach(Case IN LISTS Cases)
        string(REGEX REPLACE "^TEST\\(\"([^\"]+)\"\\).*" "\\1" Name "${Case}")
        add_test(NAME "${Name}" COMMAND ${Target} "${Name}")
        set_tests_properties("${Name}" PROPERTIES TIMEOUT 20 LABELS "physics-snapshot;${Prefix}")
    endforeach()
endfunction()

dxf_snapshot_tests(dxf_physics_snapshot_core_tests PhysicsSnapshotCoreTests.cpp snapshot_core_)
if(NOT DXF_SNAPSHOT_CORE_ONLY)
    dxf_snapshot_tests(dxf_physics_snapshot_world_tests PhysicsSnapshotWorldTests.cpp snapshot_world_)
    target_link_libraries(dxf_physics_snapshot_world_tests PRIVATE dxf::physics)
endif()
