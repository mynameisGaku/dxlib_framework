cmake_minimum_required(VERSION 3.24)
project(dxf_debug_validation LANGUAGES C CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(DXF_DEBUG_ASAN "Instrument all validation implementations with address/undefined sanitizers" OFF)
option(DXF_DEBUG_TSAN "Instrument all validation implementations with thread sanitizer" OFF)
if(DXF_DEBUG_ASAN AND DXF_DEBUG_TSAN)
    message(FATAL_ERROR "Use separate sanitizer builds")
endif()
if(DXF_DEBUG_ASAN OR DXF_DEBUG_TSAN)
    # この入口の計測はGNU形式のコンパイラーと非Windows環境を対象にする。
    if(WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR "Debug validation sanitizers require non-Windows GCC/Clang")
    endif()
    if(DXF_DEBUG_ASAN)
        set(_instrument -fsanitize=address,undefined)
    else()
        set(_instrument -fsanitize=thread)
    endif()
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "${_instrument}")
    set(CMAKE_REQUIRED_LINK_OPTIONS "${_instrument}")
    unset(DXF_DEBUG_INSTRUMENTATION_AVAILABLE CACHE)
    check_cxx_source_compiles("int main() { return 0; }" DXF_DEBUG_INSTRUMENTATION_AVAILABLE)
    if(NOT DXF_DEBUG_INSTRUMENTATION_AVAILABLE)
        message(FATAL_ERROR "Requested sanitizer compiler/runtime is unavailable")
    endif()
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    # add_subdirectoryより前に設定し、実装ライブラリ・テスト・ufbxにも適用する。
    add_compile_options(${_instrument} -fno-omit-frame-pointer)
    add_link_options(${_instrument})
endif()

# 正規の層・ソース・試験登録を使う。SDKやGUI実行ファイルを生成しない専用入口。
set(DXF_BUILD_TESTS ON CACHE BOOL "" FORCE)
foreach(_option DXF_BUILD_NATIVE DXF_BUILD_NATIVE_SMOKE DXF_RUN_DEVICE_TESTS
        DXF_BUILD_EXAMPLE DXF_BUILD_STARTER DXF_BUILD_MODEL_VIEWER DXF_BUILD_GAMEPLAY_SAMPLE DXF_BUILD_UI_SAMPLE DXF_BUILD_RENDER_DEBUG
        DXF_INSTALL DXF_SANITIZERS DXF_COVERAGE)
    set(${_option} OFF CACHE BOOL "" FORCE)
endforeach()
enable_testing()
add_subdirectory("${ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/framework")

# 旧入口の/WXを持つ4対象は厳格さを維持する。C4324だけはFVector3/FQuaternionの
# alignas(16)に由来する意図したpaddingの通知。ABIは変えずPRIVATEで限定する。
foreach(_target dxf_render_continuation_tests dxf_job_fault_tests dxf_render_views_tests dxf_native_views_tests)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /WX /wd4324)
    else()
        target_compile_options(${_target} PRIVATE -Werror)
    endif()
endforeach()

if(NOT MSVC)
    # 旧coreからPUBLIC伝播していた-Werrorも、対応する正規層と旧試験に保持する。
    foreach(_target dxf_toolbox dxf_support dxf_physics dxf_debug_tools
            dxf_debug_tools_tests dxf_debug_physics_tests dxf_transparency_tests
            dxf_transparency_fault_tests dxf_native_transparency_tests)
        target_compile_options(${_target} PRIVATE -Werror)
    endforeach()
endif()
