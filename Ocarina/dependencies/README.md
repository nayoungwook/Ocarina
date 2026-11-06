# Engine dependencies

Windows x64 SDK files imported from Tonic. Ocarina builds against this directory.

- `include/SDL3`, `SDL3_ttf`, `SDL3_mixer`: SDL headers.
- `include/GL`, `glm`, `stb`, `tinyobjloader`: graphics and model-loading headers.
- `lib/`: import/static libraries, runtime DLLs and supplied debug symbols.

CMake and `Ocarina.vcxproj` copy SDL3, SDL3_ttf, SDL3_mixer and GLEW DLLs from
`lib/` beside `Ocarina.exe`. Library and header names are unchanged; bundled
license notices are retained in the headers and source directories.

Engine shaders and example assets are in `Ocarina/engine/shaders` and
`Ocarina/engine/resources`. The build does not require the original Tonic folder.
