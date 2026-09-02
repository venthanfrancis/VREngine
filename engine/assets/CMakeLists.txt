add_library(arengine_assets STATIC
    src/Assets.cpp
    src/ImageDecode.cpp
    src/MeshDecode.cpp
)
add_library(AREngine::Assets ALIAS arengine_assets)

target_sources(arengine_assets
    PRIVATE
        include/AREngine/Assets/Assets.hpp
        include/AREngine/Assets/AssetId.hpp
        include/AREngine/Assets/TextAsset.hpp
        include/AREngine/Assets/BinaryAsset.hpp
        include/AREngine/Assets/TextureAsset.hpp
        include/AREngine/Assets/MeshAsset.hpp
        src/ImageDecode.hpp
        src/MeshDecode.hpp
)

target_include_directories(arengine_assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_assets
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_assets PUBLIC cxx_std_20)

# Deliberately not linked to Platform: std::filesystem and std::ifstream
# already provide everything M6 needs (path resolution, normalization,
# file reading) as portable standard C++ — there is no OS-specific
# behavior here for Platform to abstract. See docs/ARCHITECTURE.md,
# "Why Assets Does Not Depend On Platform".

# M14: stb_image.h is vendored directly (third_party/stb/), not fetched
# via CMake FetchContent — unlike OpenXR-SDK (this project's one other
# third-party dependency, fetched lazily only when the optional
# ARENGINE_ENABLE_OPENXR flag is on), arengine_assets builds
# unconditionally, so FetchContent here would make network access
# mandatory on every fresh configure of the whole project. See
# docs/ROADMAP.md's "No third-party dependencies until a milestone
# explicitly calls for one" and docs/ARCHITECTURE.md, "M14 - Asset-
# Backed Texture & Material Loading Foundation". PRIVATE: stb_image.h
# is an implementation detail of src/ImageDecode.cpp only, never part
# of this library's public interface.
target_include_directories(arengine_assets
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/stb
)

# M15: tiny_obj_loader.h is vendored directly (third_party/tinyobjloader/),
# same reasoning and same PRIVATE scoping as stb_image.h above - an
# implementation detail of src/MeshDecode.cpp only. See
# docs/ROADMAP.md's "No third-party dependencies until a milestone
# explicitly calls for one" and docs/ARCHITECTURE.md, "M15 - Asset-
# Backed Mesh Loading Foundation".
target_include_directories(arengine_assets
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/tinyobjloader
)
