#pragma once

#include "AREngine/Assets/AssetId.hpp"
#include "AREngine/Assets/BinaryAsset.hpp"
#include "AREngine/Assets/TextAsset.hpp"
#include "AREngine/Assets/TextureAsset.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace AREngine::Assets
{
    // Owns one asset root directory and every asset loaded relative to
    // it. Loads raw text/binary content, and decoded RGBA8 image
    // content (M14), from disk, assigns each a distinct AssetId, and
    // caches by resolved path so repeated loads of the same file don't
    // hit the disk twice. See docs/ARCHITECTURE.md, "M6 Implementation
    // Notes" for the asset root model, cache key strategy, and failure
    // model, and "M14 - Asset-Backed Texture & Material Loading
    // Foundation" for LoadTexture/TextureAsset specifically.
    //
    // Depends only on Core — see docs/ARCHITECTURE.md, "Why Assets
    // Does Not Depend On Platform". No Vulkan/OpenXR/graphics-backend
    // concepts leak into this header even with image decoding added -
    // TextureAsset is CPU pixel data only. No Scene integration, no
    // Runtime ownership requirement.
    //
    // Deliberately not a singleton: an application owns an AssetManager
    // instance explicitly, the same ownership philosophy used
    // everywhere else in the engine.
    //
    // Two failure philosophies, matching the precedent from M4/M5:
    // LoadText/LoadBinary (which read real files that could plausibly
    // be missing, unreadable, or path-escaping) reject predictably via
    // std::optional, never crash. GetText/GetBinary (queries on an
    // AssetId the caller should already know is valid) assert on an
    // unknown id instead — call IsValid() first if unsure.
    class AssetManager
    {
    public:
        // `root` must already exist and be a directory. Asserts
        // otherwise — a nonexistent asset root is a setup/caller bug,
        // not a normal runtime condition worth a recovery path.
        explicit AssetManager(const std::filesystem::path& root);

        // Loads (or returns the cached AssetId for) the text file at
        // `relativePath`, resolved against the asset root. Returns
        // std::nullopt if the file doesn't exist, can't be read, or
        // the resolved path would escape the asset root.
        [[nodiscard]] std::optional<AssetId> LoadText(const std::filesystem::path& relativePath);

        // Same as LoadText, but for raw bytes. The same relativePath
        // loaded via both LoadText and LoadBinary produces two
        // independent assets with two independent AssetIds — see
        // docs/ARCHITECTURE.md, "Same-Path/Different-Type Behavior".
        [[nodiscard]] std::optional<AssetId> LoadBinary(const std::filesystem::path& relativePath);

        // M14: loads (or returns the cached AssetId for) the image file
        // at `relativePath`, decoded and normalized to RGBA8. Returns
        // std::nullopt if the file doesn't exist, can't be read, the
        // resolved path would escape the asset root, or the file's
        // contents can't be decoded as a supported image format (corrupt
        // file, zero-size, unsupported format) - one consistent failure
        // path for all of these, same philosophy as LoadText/LoadBinary.
        // A third independent {path, type} cache, same shape as text and
        // binary - see docs/ARCHITECTURE.md, "Same-Path/Different-Type
        // Behavior".
        [[nodiscard]] std::optional<AssetId> LoadTexture(const std::filesystem::path& relativePath);

        [[nodiscard]] bool IsValid(AssetId id) const;

        [[nodiscard]] const TextAsset& GetText(AssetId id) const;
        [[nodiscard]] const BinaryAsset& GetBinary(AssetId id) const;
        [[nodiscard]] const TextureAsset& GetTexture(AssetId id) const;

        [[nodiscard]] const std::filesystem::path& GetRoot() const { return m_root; }

    private:
        // Resolves `relativePath` against the asset root, normalizes
        // it, and confirms it does not escape the root. Returns
        // std::nullopt (not an assert) on any failure — an untrusted or
        // malformed relative path is exactly the kind of input this
        // function exists to reject predictably. See
        // docs/ARCHITECTURE.md, "Root Traversal Handling".
        [[nodiscard]] std::optional<std::filesystem::path> ResolvePath(const std::filesystem::path& relativePath) const;

        std::filesystem::path m_root;
        std::uint64_t m_nextAssetId = 1;

        // Cache key: the generic_string() of the fully resolved,
        // normalized absolute path — see docs/ARCHITECTURE.md,
        // "Normalization Strategy" for what this does and does not
        // guarantee (e.g. case sensitivity).
        std::unordered_map<std::string, AssetId> m_textPathToId;
        std::unordered_map<AssetId, TextAsset> m_textAssets;

        std::unordered_map<std::string, AssetId> m_binaryPathToId;
        std::unordered_map<AssetId, BinaryAsset> m_binaryAssets;

        std::unordered_map<std::string, AssetId> m_texturePathToId;
        std::unordered_map<AssetId, TextureAsset> m_textureAssets;
    };
}
