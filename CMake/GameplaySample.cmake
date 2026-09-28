# SPDX-License-Identifier: NOASSERTION
# GameplaySampleの開発用の例。GUI入口は専用のディレクトリで定義し、本体のcppを表示専用で載せる。
# 関数（dxf_runtime_pathsなど）の定義後に読み込むため、includeはルートの後段で行う。
if(DXF_BUILD_GAMEPLAY_SAMPLE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_GAMEPLAY_SAMPLE requires DXF_BUILD_NATIVE=ON")
    endif()
    add_subdirectory(CMake/GameplaySampleApp)
endif()
