# Writes a tiny per-exe development path settings file.
# Required variables: EXE_DIR (absolute), ROOT (absolute), OUTPUT (file to write).
# Relative paths keep working after moving the whole folder on the same volume.
# The file is rewritten only when the content changes.
file(RELATIVE_PATH ProjectRootRelative "${EXE_DIR}" "${ROOT}")
set(SettingsContent "# Auto-generated development settings. Do not edit.\nVersion=1\nMode=Development\nProjectRootRelative=${ProjectRootRelative}\n")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" PreviousContent)
    if(PreviousContent STREQUAL SettingsContent)
        return()
    endif()
endif()
file(WRITE "${OUTPUT}" "${SettingsContent}")
