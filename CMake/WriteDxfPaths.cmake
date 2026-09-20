# Writes a tiny per-exe development path settings file.
# Required variables: EXE_DIR (absolute), ROOT (absolute), OUTPUT (file to write).
# Relative paths keep working after moving the whole folder on the same volume.
file(RELATIVE_PATH ProjectRootRelative "${EXE_DIR}" "${ROOT}")
file(WRITE "${OUTPUT}" "# Auto-generated development settings. Do not edit.\nVersion=1\nMode=Development\nProjectRootRelative=${ProjectRootRelative}\n")
