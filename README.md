部分検証用の依存ソースです。リポジトリ本体へ上書きしないでください。
cmake -S Tools/DebugValidation -B Build/DebugTools -DCMAKE_BUILD_TYPE=Release
cmake --build Build/DebugTools
ctest --test-dir Build/DebugTools --output-on-failure
