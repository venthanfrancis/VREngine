# tiny_obj_loader.h

Vendored verbatim (unmodified) from the official
[tinyobjloader/tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
repository.

- Commit: `62ff207968f3dc14a64a1e2378dce67b760e7c4a`
- Source: https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/62ff207968f3dc14a64a1e2378dce67b760e7c4a/tiny_obj_loader.h
- License: MIT (full text embedded at the top of `tiny_obj_loader.h`)

Vendored directly rather than pulled via CMake `FetchContent`, for the
same reason as `third_party/stb/stb_image.h`: `engine/assets` (the only
consumer) builds unconditionally — unlike this project's one other
third-party dependency (OpenXR-SDK, fetched lazily only when
`ARENGINE_ENABLE_OPENXR=ON`), making tinyobjloader a mandatory
dependency via `FetchContent` would otherwise force network access on
every fresh configure of the whole project. See `docs/ARCHITECTURE.md`,
"M15 - Asset-Backed Mesh Loading Foundation".

Used only by `engine/assets/src/MeshDecode.cpp`, which is the single
translation unit defining `TINYOBJLOADER_IMPLEMENTATION` and calling
tinyobjloader's simple, filename-based `tinyobj::LoadObj` (not the
stream/callback-based `LoadObjWithCallback` — the simple API's indices
are already normalized to 0-based with an unambiguous `-1` "absent"
sentinel; the callback API hands back raw, differently-sentineled
indices and would require reimplementing that normalization). No other
file in this repository includes `tiny_obj_loader.h` directly.

`.mtl` material files are never loaded — `MeshDecode.cpp` passes no
material file map / base directory and discards `LoadObj`'s returned
material list entirely, matching this milestone's explicit scope (no
`.mtl`-derived `MaterialId`).

## MSVC/C++20 compile workaround

This pinned commit's embedded `fast_float` snapshot fails to compile
under MSVC in C++20 mode with error C3615 (`loop_parse_if_eight_digits`
guarded behind a `constexpr` that MSVC's own checker rejects, even
though MSVC's STL already advertises the feature-test macro that gates
it — a genuine upstream fast_float/MSVC incompatibility, not an
AREngine defect). `MeshDecode.cpp` works around this from its own
translation unit (not by modifying this vendored file, which stays
verbatim) by undefining `__cpp_lib_constexpr_algorithms` immediately
before including `tiny_obj_loader.h`, which steers the header's own
existing feature-test check onto its non-constexpr fallback path — see
the comment in `MeshDecode.cpp` for the full explanation.
