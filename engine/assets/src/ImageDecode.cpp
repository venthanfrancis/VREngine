#include "ImageDecode.hpp"

// The one translation unit in this codebase that defines
// STB_IMAGE_IMPLEMENTATION - stb_image.h must never be #included with
// this macro defined anywhere else, or the implementation would be
// duplicated across translation units. See third_party/stb/README.md
// for provenance (v2.30, commit 013ac3beddff3dbffafd5177e7972067cd2b5083).
//
// Wrapped in a warning-suppression pair so stb's own internal warnings
// never leak into this project's /W4 build - this fences THIRD-PARTY
// code only, it does not weaken AREngine's own warning level anywhere
// else in the codebase.
#if defined(_MSC_VER)
    #pragma warning(push, 0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO // AREngine always decodes from an in-memory buffer (AssetManager already read the file) - no stb-side file I/O needed
#include "stb_image.h"
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

namespace AREngine::Assets
{
    std::optional<TextureAsset> DecodeImageRGBA8(std::span<const std::byte> fileBytes)
    {
        if (fileBytes.empty())
        {
            return std::nullopt;
        }

        int width = 0;
        int height = 0;
        int sourceChannels = 0;
        // desired_channels=4 makes stb_image itself normalize to RGBA8
        // regardless of the source file's actual channel count - no
        // separate conversion step needed on AREngine's side.
        unsigned char* decoded = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(fileBytes.data()), static_cast<int>(fileBytes.size()),
            &width, &height, &sourceChannels, 4);

        if (decoded == nullptr || width <= 0 || height <= 0)
        {
            return std::nullopt;
        }

        TextureAsset asset;
        asset.width = static_cast<std::uint32_t>(width);
        asset.height = static_cast<std::uint32_t>(height);
        asset.channels = 4;
        asset.pixels.assign(decoded, decoded + (static_cast<std::size_t>(width) * height * 4));

        stbi_image_free(decoded); // independently-owned copy already made above - no stb pointer escapes this function

        return asset;
    }
}
