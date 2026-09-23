# Builds the DxLib core library (DxLib_*.lib) from the official DxLibMake source package for x64.
# The source list is read from the official DxLibMake.vcxproj instead of being copied by hand.
# This project never downloads anything and never accepts third-party license terms.
cmake_minimum_required(VERSION 3.24)
project(DxLibFbxBuild LANGUAGES CXX)

set(DXF_DXLIB_SOURCE_DIR "" CACHE PATH "Extracted official DxLibMake directory (contains DxLibMake.vcxproj)")
option(DXF_DXLIB_WITH_FBX "Compile DxLib with DX_LOAD_FBX_MODEL (requires DXF_FBX_SDK_ROOT)" ON)
set(DXF_FBX_SDK_ROOT "" CACHE PATH "Installed Autodesk FBX SDK root (contains include/fbxsdk.h)")

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The DxLib source build supports Windows/MSVC x64 only")
endif()
if(NOT EXISTS "${DXF_DXLIB_SOURCE_DIR}/DxLibMake.vcxproj")
    message(FATAL_ERROR "DXF_DXLIB_SOURCE_DIR must point to the extracted DxLibMake directory")
endif()
if(DXF_DXLIB_WITH_FBX AND NOT EXISTS "${DXF_FBX_SDK_ROOT}/include/fbxsdk.h")
    message(FATAL_ERROR "DXF_DXLIB_WITH_FBX=ON requires DXF_FBX_SDK_ROOT with include/fbxsdk.h")
endif()

# The official static libraries use the static CRT; keep /MT and /MTd.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(READ "${DXF_DXLIB_SOURCE_DIR}/DxLibMake.vcxproj" _dxf_project)
string(REGEX MATCHALL "ClCompile Include=\"[^\"]+\"" _dxf_items "${_dxf_project}")
set(_dxf_sources)
foreach(_dxf_item IN LISTS _dxf_items)
    string(REGEX REPLACE "ClCompile Include=\"([^\"]+)\"" "\\1" _dxf_path "${_dxf_item}")
    string(REPLACE "\\" "/" _dxf_path "${_dxf_path}")
    list(APPEND _dxf_sources "${DXF_DXLIB_SOURCE_DIR}/${_dxf_path}")
endforeach()
list(LENGTH _dxf_sources _dxf_count)
if(_dxf_count EQUAL 0)
    message(FATAL_ERROR "No ClCompile entries were found in DxLibMake.vcxproj")
endif()
message(STATUS "DxLib core sources from DxLibMake.vcxproj: ${_dxf_count}")

# DxLib's own sources are Shift_JIS; fix the code page per file so the build does not depend on the machine locale.
set_source_files_properties(${_dxf_sources} PROPERTIES COMPILE_OPTIONS "/source-charset:.932;/execution-charset:.932")
# Name-mangling bridge for the official v140-built DxUseCLib (see the file header). UTF-8, our own code.
set(_dxf_bridge "${CMAKE_CURRENT_LIST_DIR}/DxLibV140AbiBridge.cpp")
set_source_files_properties("${_dxf_bridge}" PROPERTIES COMPILE_OPTIONS "/utf-8")
add_library(DxLibCore STATIC ${_dxf_sources} "${_dxf_bridge}")
set_target_properties(DxLibCore PROPERTIES
    OUTPUT_NAME_RELEASE "DxLib_fbx_x64_MT"
    OUTPUT_NAME_DEBUG "DxLib_fbx_x64_MTd"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
target_include_directories(DxLibCore PRIVATE "${DXF_DXLIB_SOURCE_DIR}" "${DXF_DXLIB_SOURCE_DIR}/Windows")
# Same defines as the official project (WIN32;_LIB;_DEBUG|NDEBUG). _WIN64 comes from the x64 compiler.
target_compile_definitions(DxLibCore PRIVATE WIN32 _LIB $<$<CONFIG:Debug>:_DEBUG> $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)
target_compile_options(DxLibCore PRIVATE /W3 /EHsc $<$<NOT:$<CONFIG:Debug>>:/O2 /Oi /GS->)
if(DXF_DXLIB_WITH_FBX)
    # DX_LOAD_FBX_MODEL must be set when DxLib itself is compiled; an application-side define has no effect.
    # Static libfbxsdk-mt: FBXSDK_SHARED must stay undefined (the SDK tests it with #ifdef).
    target_compile_definitions(DxLibCore PRIVATE DX_LOAD_FBX_MODEL)
    target_include_directories(DxLibCore PRIVATE "${DXF_FBX_SDK_ROOT}/include")
endif()
