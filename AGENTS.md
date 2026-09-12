# Repository conventions

Follow [Docs/CodingStandard.md](Docs/CodingStandard.md) for every C++ change.

- Do not use STL in Source, Examples, or Tests. Add needed functionality to Toolbox; an alias or wrapper around an STL implementation is not a replacement.
- Only Toolbox may use the C++ language support headers `<new>` and `<initializer_list>` and their `std::align_val_t` / `std::initializer_list` types. Keep OS/C runtime and SIMD dependencies at the appropriate implementation boundary.
- Use `int32`, `int64`, other fixed-width integers, and `f32` / `f64`. Keep native types only where defining these aliases or matching platform ABI requires them.
- Explain header declarations with concise Japanese multiline `/** ... */` comments. Use `//` comments inside function bodies and throughout cpp files. Document parameter roles in the header function comment.
- Use Allman braces, one statement per line, and separate variable declarations. Apply the repository `.clang-format` configuration.
- Keep the root Visual Studio solution focused on Toolbox, the framework layers, Sandbox, and Starter. Development/test projects belong to `GenerateProjectFiles.bat -Development`.
- Run `python Tools/CheckNoStl.py` and appropriate tests after changes. Keep current API documentation consistent; do not rewrite historical validation results as if they describe a new run.
