# Official VC package only. No SDK is downloaded implicitly by CMake.
include(FindPackageHandleStandardArgs)
if(NOT DXLIB_ROOT AND DEFINED ENV{DXLIB_ROOT})
    set(DXLIB_ROOT "$ENV{DXLIB_ROOT}")
endif()
set(_dxf_hints "${DXLIB_ROOT}")
if(DXLIB_ROOT)
    file(GLOB _dxf_children LIST_DIRECTORIES true "${DXLIB_ROOT}/*" "${DXLIB_ROOT}/*/*")
    list(APPEND _dxf_hints ${_dxf_children})
endif()
find_path(DxLib_INCLUDE_DIR DxLib.h HINTS ${_dxf_hints} NO_DEFAULT_PATH)
if(DxLib_INCLUDE_DIR)
    file(GLOB DxLib_LIBRARIES "${DxLib_INCLUDE_DIR}/*.lib")
endif()
find_package_handle_standard_args(DxLib REQUIRED_VARS DxLib_INCLUDE_DIR DxLib_LIBRARIES
    REASON_FAILURE_MESSAGE "Set DXLIB_ROOT to the extracted official VC SDK (headers and .lib files are both required).")
if(DxLib_FOUND AND NOT TARGET DxLib::SDK)
    add_library(DxLib::SDK INTERFACE IMPORTED)
    # DxLib.h emits the version/architecture-specific MSVC autolink directives.
    set_target_properties(DxLib::SDK PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DxLib_INCLUDE_DIR}"
        INTERFACE_LINK_DIRECTORIES "${DxLib_INCLUDE_DIR}")
endif()
mark_as_advanced(DxLib_INCLUDE_DIR)
