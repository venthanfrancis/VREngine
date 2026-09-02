#include "AREngine/Assets/Assets.hpp"

#include "ImageDecode.hpp"
#include "MeshDecode.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

#include <format>
#include <fstream>
#include <sstream>

namespace AREngine::Assets
{
    namespace
    {
        std::optional<std::string> ReadTextFile(const std::filesystem::path& path)
        {
            // Opened in binary mode even for "text" so contents are an
            // exact byte-for-byte copy of the file — no CRLF-to-LF
            // translation to make caching/testing less predictable.
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open())
            {
                return std::nullopt;
            }

            std::ostringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        std::optional<std::vector<std::byte>> ReadBinaryFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open())
            {
                return std::nullopt;
            }

            const std::streamsize size = file.tellg();
            if (size < 0)
            {
                return std::nullopt;
            }
            file.seekg(0, std::ios::beg);

            std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size))
            {
                return std::nullopt;
            }

            return bytes;
        }
    }

    AssetManager::AssetManager(const std::filesystem::path& root)
    {
        std::error_code ec;
        m_root = std::filesystem::canonical(root, ec);
        AR_ASSERT_MSG(!ec, "AssetManager's asset root must already exist and be accessible");
    }

    std::optional<std::filesystem::path> AssetManager::ResolvePath(const std::filesystem::path& relativePath) const
    {
        // An absolute input path is already a misuse — asset paths are
        // defined to be relative to the root (see the WORKED example in
        // docs/ARCHITECTURE.md), not a way to reach arbitrary files.
        if (relativePath.is_absolute())
        {
            return std::nullopt;
        }

        std::error_code ec;
        const std::filesystem::path normalized = std::filesystem::weakly_canonical(m_root / relativePath, ec);
        if (ec)
        {
            return std::nullopt;
        }

        std::error_code relEc;
        const std::filesystem::path rel = std::filesystem::relative(normalized, m_root, relEc);
        if (relEc || rel.empty())
        {
            return std::nullopt;
        }

        // relative() returns a path starting with ".." when `normalized`
        // falls outside m_root's subtree — that's the escape case.
        if (rel.begin() != rel.end() && *rel.begin() == "..")
        {
            AR_LOG_WARNING(std::format("Asset path escapes the asset root, rejected: {}", relativePath.string()));
            return std::nullopt;
        }

        return normalized;
    }

    std::optional<AssetId> AssetManager::LoadText(const std::filesystem::path& relativePath)
    {
        const std::optional<std::filesystem::path> resolved = ResolvePath(relativePath);
        if (!resolved.has_value())
        {
            return std::nullopt;
        }

        const std::string cacheKey = resolved->generic_string();
        const auto cached = m_textPathToId.find(cacheKey);
        if (cached != m_textPathToId.end())
        {
            return cached->second;
        }

        std::optional<std::string> contents = ReadTextFile(*resolved);
        if (!contents.has_value())
        {
            AR_LOG_WARNING(std::format("Failed to load text asset: {}", relativePath.string()));
            return std::nullopt;
        }

        const AssetId id{m_nextAssetId++};
        TextAsset asset;
        asset.id = id;
        asset.path = relativePath;
        asset.contents = std::move(*contents);

        m_textAssets.emplace(id, std::move(asset));
        m_textPathToId.emplace(cacheKey, id);

        return id;
    }

    std::optional<AssetId> AssetManager::LoadBinary(const std::filesystem::path& relativePath)
    {
        const std::optional<std::filesystem::path> resolved = ResolvePath(relativePath);
        if (!resolved.has_value())
        {
            return std::nullopt;
        }

        const std::string cacheKey = resolved->generic_string();
        const auto cached = m_binaryPathToId.find(cacheKey);
        if (cached != m_binaryPathToId.end())
        {
            return cached->second;
        }

        std::optional<std::vector<std::byte>> bytes = ReadBinaryFile(*resolved);
        if (!bytes.has_value())
        {
            AR_LOG_WARNING(std::format("Failed to load binary asset: {}", relativePath.string()));
            return std::nullopt;
        }

        const AssetId id{m_nextAssetId++};
        BinaryAsset asset;
        asset.id = id;
        asset.path = relativePath;
        asset.bytes = std::move(*bytes);

        m_binaryAssets.emplace(id, std::move(asset));
        m_binaryPathToId.emplace(cacheKey, id);

        return id;
    }

    std::optional<AssetId> AssetManager::LoadTexture(const std::filesystem::path& relativePath)
    {
        const std::optional<std::filesystem::path> resolved = ResolvePath(relativePath);
        if (!resolved.has_value())
        {
            return std::nullopt;
        }

        const std::string cacheKey = resolved->generic_string();
        const auto cached = m_texturePathToId.find(cacheKey);
        if (cached != m_texturePathToId.end())
        {
            return cached->second;
        }

        // Reads bytes independently here (via the same ReadBinaryFile
        // helper LoadBinary itself uses) rather than calling the PUBLIC
        // LoadBinary and decoding its result: routing through LoadBinary
        // would mint a second, throwaway AssetId for what's really just
        // a decode input, and would pollute the binary cache with large
        // intermediate file content nothing else needs to reference by
        // AssetId. This keeps LoadTexture structurally parallel to
        // LoadText/LoadBinary (its own independent {path, type} cache),
        // not a wrapper built on top of LoadBinary's own cache.
        std::optional<std::vector<std::byte>> bytes = ReadBinaryFile(*resolved);
        if (!bytes.has_value())
        {
            AR_LOG_WARNING(std::format("Failed to load texture asset: {}", relativePath.string()));
            return std::nullopt;
        }

        std::optional<TextureAsset> decoded = DecodeImageRGBA8(*bytes);
        if (!decoded.has_value())
        {
            AR_LOG_WARNING(std::format("Failed to decode texture asset (corrupt or unsupported image format): {}", relativePath.string()));
            return std::nullopt;
        }

        const AssetId id{m_nextAssetId++};
        decoded->id = id;
        decoded->path = relativePath;

        m_textureAssets.emplace(id, std::move(*decoded));
        m_texturePathToId.emplace(cacheKey, id);

        return id;
    }

    std::optional<AssetId> AssetManager::LoadMesh(const std::filesystem::path& relativePath)
    {
        const std::optional<std::filesystem::path> resolved = ResolvePath(relativePath);
        if (!resolved.has_value())
        {
            return std::nullopt;
        }

        const std::string cacheKey = resolved->generic_string();
        const auto cached = m_meshPathToId.find(cacheKey);
        if (cached != m_meshPathToId.end())
        {
            return cached->second;
        }

        // Deliberate exception to LoadText/LoadBinary/LoadTexture's
        // "read bytes ourselves, then decode from memory" shape:
        // DecodeMeshOBJ takes the resolved path directly and reads the
        // file itself via tinyobjloader's own file-based LoadObj - see
        // MeshDecode.hpp's header comment for why.
        std::optional<MeshAsset> decoded = DecodeMeshOBJ(*resolved);
        if (!decoded.has_value())
        {
            AR_LOG_WARNING(std::format("Failed to load mesh asset (missing, unreadable, or unparsable OBJ): {}", relativePath.string()));
            return std::nullopt;
        }

        const AssetId id{m_nextAssetId++};
        decoded->id = id;
        decoded->path = relativePath;

        m_meshAssets.emplace(id, std::move(*decoded));
        m_meshPathToId.emplace(cacheKey, id);

        return id;
    }

    bool AssetManager::IsValid(AssetId id) const
    {
        return id.IsValid() &&
            (m_textAssets.contains(id) || m_binaryAssets.contains(id) ||
             m_textureAssets.contains(id) || m_meshAssets.contains(id));
    }

    const TextAsset& AssetManager::GetText(AssetId id) const
    {
        const auto it = m_textAssets.find(id);
        AR_ASSERT_MSG(it != m_textAssets.end(), "GetText called with an id that is not a known text asset - check IsValid() first");
        return it->second;
    }

    const BinaryAsset& AssetManager::GetBinary(AssetId id) const
    {
        const auto it = m_binaryAssets.find(id);
        AR_ASSERT_MSG(it != m_binaryAssets.end(), "GetBinary called with an id that is not a known binary asset - check IsValid() first");
        return it->second;
    }

    const TextureAsset& AssetManager::GetTexture(AssetId id) const
    {
        const auto it = m_textureAssets.find(id);
        AR_ASSERT_MSG(it != m_textureAssets.end(), "GetTexture called with an id that is not a known texture asset - check IsValid() first");
        return it->second;
    }

    const MeshAsset& AssetManager::GetMesh(AssetId id) const
    {
        const auto it = m_meshAssets.find(id);
        AR_ASSERT_MSG(it != m_meshAssets.end(), "GetMesh called with an id that is not a known mesh asset - check IsValid() first");
        return it->second;
    }
}
