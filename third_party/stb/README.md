# stb_image.h

Vendored verbatim (unmodified) from the official
[nothings/stb](https://github.com/nothings/stb) repository.

- Version: v2.30 (embedded version marker inside the file)
- Commit: `013ac3beddff3dbffafd5177e7972067cd2b5083`
- Source: https://raw.githubusercontent.com/nothings/stb/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h
- License: dual MIT / public domain (full text embedded at the bottom
  of `stb_image.h`)

Vendored directly rather than pulled via CMake `FetchContent`, since
`engine/assets` (the only consumer) builds unconditionally — unlike
this project's one other third-party dependency (OpenXR-SDK, fetched
lazily only when `ARENGINE_ENABLE_OPENXR=ON`), making stb a mandatory
dependency would otherwise force network access on every fresh
configure of the whole project. See `docs/ARCHITECTURE.md`, "M14 -
Asset-Backed Texture & Material Loading Foundation".

Used only by `engine/assets/src/ImageDecode.cpp`, which is the single
translation unit defining `STB_IMAGE_IMPLEMENTATION`. No other file in
this repository includes `stb_image.h` directly.
