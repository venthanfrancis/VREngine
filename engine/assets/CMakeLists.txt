add_library(arengine_assets STATIC
    src/Assets.cpp
)
add_library(AREngine::Assets ALIAS arengine_assets)

target_sources(arengine_assets
    PRIVATE
        include/AREngine/Assets/Assets.hpp
        include/AREngine/Assets/AssetId.hpp
        include/AREngine/Assets/TextAsset.hpp
        include/AREngine/Assets/BinaryAsset.hpp
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
