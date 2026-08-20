# Engine modules, ordered from least to most dependent (see docs/ARCHITECTURE.md
# for the full dependency graph). Each module is its own static library target.

add_subdirectory(core)
add_subdirectory(frame)
add_subdirectory(platform)
add_subdirectory(rendering)
add_subdirectory(scene)
add_subdirectory(assets)
add_subdirectory(input)
add_subdirectory(xr)
