# Apply Multithreading Stage 1 + Stage 2

Run from a **clean** `dxlib_framework` checkout whose current branch contains:

`c75e7bb85a33865ba6b4d07c0064646d2c402408`

The intended branch is `physics/continuation-89ec1f0`.

## Apply

Copy this bundle somewhere outside the repository, then from the repository root run:

```powershell
python C:\path\to\bundle\apply_stage2.py
```

The script first applies the bundled Stage-1 patch if `Toolbox/JobSystem.h` is not already present, then copies Stage-2 files and edits the c75e7bb integration points. It refuses a dirty worktree and aborts on any unexpected source anchor instead of guessing.

## Validate before commit

```powershell
python Tools\CheckNoStl.py

cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure

cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

If the Linux/WSL toolchains are available, also run:

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug --output-on-failure

cmake --preset portable-release
cmake --build --preset portable-release
ctest --preset portable-release --output-on-failure

cmake --preset sanitized
cmake --build --preset sanitized
ctest --preset sanitized --output-on-failure

cmake --preset thread-sanitized
cmake --build --preset thread-sanitized
ctest --preset thread-sanitized --output-on-failure
```

Also run the direct test executables if the repository validation scripts normally record their internal case counts.

## Commit and push only after Green

```powershell
git diff --check
git status --short
git add Source CMake CMakeLists.txt CMakePresets.json Tests Tools Docs .github
git commit -m "feat: add deterministic multithreaded physics execution"
git push origin HEAD:physics/continuation-89ec1f0
```

Do not push to `main` and do not force push.
