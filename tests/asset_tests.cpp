// M6 automated tests for AREngine::Assets: AssetId, AssetManager's
// text/binary loading, caching, path normalization, root-traversal
// protection, and same-path/different-type behavior. Plus M14's
// LoadTexture/GetTexture (decoded RGBA8 image loading) and M15's
// LoadMesh/GetMesh (decoded/unified OBJ mesh loading). No human
// interaction, fully headless — reads tiny fixture files from
// tests/data/assets/ (path supplied by CMake via AR_TEST_ASSETS_ROOT,
// so no machine-specific path is hard-coded here). tests/data/assets/
// now holds both test-only fixtures AND the small PNGs/OBJs the
// desktop/XR demos load at runtime (tests/data/assets/textures/,
// tests/data/assets/meshes/) - see docs/ARCHITECTURE.md, "M14 - Asset-
// Backed Texture & Material Loading Foundation" and "M15 - Asset-
// Backed Mesh Loading Foundation".

#include "AREngine/Assets/Assets.hpp"

#include <cstdio>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    using namespace AREngine::Assets;

    AssetManager MakeTestManager()
    {
        return AssetManager(std::filesystem::path(AR_TEST_ASSETS_ROOT));
    }

    void TestManagerCreation()
    {
        AssetManager manager = MakeTestManager();
        Check(std::filesystem::exists(manager.GetRoot()), "AssetManager can be created with a real asset root");
    }

    void TestTextLoadSucceedsWithCorrectContents()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadText("hello.txt");
        Check(id.has_value(), "A valid text file loads successfully");
        Check(id->IsValid(), "The returned AssetId is valid");

        const TextAsset& asset = manager.GetText(*id);
        Check(asset.contents == "Hello, AREngine!", "Loaded text contents are exactly correct");
    }

    void TestBinaryLoadSucceedsWithCorrectBytes()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadBinary("sample.bin");
        Check(id.has_value(), "A valid binary file loads successfully");

        const BinaryAsset& asset = manager.GetBinary(*id);
        const std::vector<std::byte> expected{
            std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
            std::byte{0xFF}, std::byte{0x00}, std::byte{0x7A}
        };
        Check(asset.bytes == expected, "Loaded binary bytes are exactly correct");
    }

    void TestMissingFileFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto textResult = manager.LoadText("does_not_exist.txt");
        Check(!textResult.has_value(), "Loading a missing text file returns std::nullopt, not a crash");

        const auto binaryResult = manager.LoadBinary("does_not_exist.bin");
        Check(!binaryResult.has_value(), "Loading a missing binary file returns std::nullopt, not a crash");
    }

    void TestInvalidAssetId()
    {
        AssetManager manager = MakeTestManager();

        const AssetId invalid{};
        Check(!invalid.IsValid(), "A default-constructed AssetId is never valid");
        Check(!manager.IsValid(invalid), "AssetManager agrees a default-constructed AssetId is invalid");

        const AssetId neverIssued{9999};
        Check(!manager.IsValid(neverIssued), "An id the manager never issued is not valid");
    }

    void TestDistinctAssetsGetDistinctIds()
    {
        AssetManager manager = MakeTestManager();

        const auto first = manager.LoadText("hello.txt");
        const auto second = manager.LoadText("second.txt");

        Check(first.has_value() && second.has_value(), "Both distinct text assets load");
        Check(!(*first == *second), "Two different text assets receive different AssetIds");
    }

    void TestCachingReturnsSameIdentity()
    {
        AssetManager manager = MakeTestManager();

        const auto first = manager.LoadText("hello.txt");
        const auto second = manager.LoadText("hello.txt");

        Check(first.has_value() && second.has_value(), "Both loads succeed");
        Check(*first == *second, "Loading the same asset twice returns the same cached AssetId");
    }

    void TestNormalizedPathDoesNotDuplicateCache()
    {
        AssetManager manager = MakeTestManager();

        const auto plain = manager.LoadText("hello.txt");
        const auto dotted = manager.LoadText("./hello.txt");

        Check(plain.has_value() && dotted.has_value(), "Both path spellings load successfully");
        Check(*plain == *dotted, "\"hello.txt\" and \"./hello.txt\" resolve to the same cached asset");
    }

    void TestRootTraversalIsRejected()
    {
        AssetManager manager = MakeTestManager();

        const auto escapeAttempt = manager.LoadText("../outside.txt");
        Check(!escapeAttempt.has_value(), "A path that escapes the asset root (..) is rejected, not loaded");

        const auto deeperEscapeAttempt = manager.LoadText("subdir/../../outside.txt");
        Check(!deeperEscapeAttempt.has_value(), "A path that escapes the root via a deeper ../.. is also rejected");

        // An absolute path is rejected outright, even one that happens
        // to point back inside the root — asset paths must be relative
        // by construction, not merely "resolve to somewhere valid."
        const auto absoluteAttempt = manager.LoadText(manager.GetRoot() / "hello.txt");
        Check(!absoluteAttempt.has_value(), "An absolute path is rejected, even if it points inside the root");
    }

    void TestSamePathAsTextAndBinaryAreIndependent()
    {
        AssetManager manager = MakeTestManager();

        const auto asText = manager.LoadText("hello.txt");
        const auto asBinary = manager.LoadBinary("hello.txt");

        Check(asText.has_value() && asBinary.has_value(),
              "The same path can be loaded as both text and binary");
        Check(!(*asText == *asBinary),
              "Loading one path as text and as binary produces two independent AssetIds (documented behavior)");

        // Loading the same path as the same type again still hits that
        // type's own cache.
        const auto asTextAgain = manager.LoadText("hello.txt");
        Check(*asText == *asTextAgain, "Re-loading as text still hits the text cache, unaffected by the binary load");
    }

    // --- M14: LoadTexture / GetTexture ---

    void TestTextureLoadSucceedsWithCorrectDimensions()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadTexture("textures/checker_red.png");
        Check(id.has_value(), "A valid PNG file loads successfully");
        Check(id->IsValid(), "The returned AssetId is valid");

        const TextureAsset& asset = manager.GetTexture(*id);
        Check(asset.width == 16, "Decoded texture width is correct");
        Check(asset.height == 16, "Decoded texture height is correct");
        Check(asset.channels == 4, "Decoded texture is normalized to 4 channels (RGBA8)");
        Check(asset.pixels.size() == static_cast<std::size_t>(asset.width) * asset.height * 4,
              "Decoded pixel buffer size is exactly width * height * 4 bytes");
    }

    void TestTextureLoadDecodesPlausibleColorContent()
    {
        // Not an exact-byte-match test (PNG re-encoding/decoding through
        // a real codec is not guaranteed bit-identical to the source
        // Bitmap that generated the fixture) - just confirms the decoded
        // pixels are plausible, non-degenerate color data: fully opaque,
        // and not literally all-zero (a common decode-gone-wrong symptom).
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadTexture("textures/checker_red.png");
        Check(id.has_value(), "checker_red.png loads");
        const TextureAsset& asset = manager.GetTexture(*id);

        bool anyNonZero = false;
        bool everyAlphaOpaque = true;
        for (std::size_t i = 0; i + 3 < asset.pixels.size(); i += 4)
        {
            if (asset.pixels[i] != 0 || asset.pixels[i + 1] != 0 || asset.pixels[i + 2] != 0) { anyNonZero = true; }
            if (asset.pixels[i + 3] != 255) { everyAlphaOpaque = false; }
        }
        Check(anyNonZero, "Decoded checker_red.png contains actual non-zero color data, not a blank/failed decode");
        Check(everyAlphaOpaque, "Decoded checker_red.png's alpha channel is fully opaque, as generated");
    }

    void TestTextureMissingFileFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto result = manager.LoadTexture("textures/does_not_exist.png");
        Check(!result.has_value(), "Loading a missing texture file returns std::nullopt, not a crash");
    }

    void TestTextureCorruptFileFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto result = manager.LoadTexture("textures/corrupt.png");
        Check(!result.has_value(), "Loading a file with a .png extension but non-image contents returns std::nullopt, not a crash");
    }

    void TestTextureCachingReturnsSameIdentity()
    {
        AssetManager manager = MakeTestManager();

        const auto first = manager.LoadTexture("textures/checker_red.png");
        const auto second = manager.LoadTexture("textures/checker_red.png");

        Check(first.has_value() && second.has_value(), "Both loads succeed");
        Check(*first == *second, "Loading the same texture twice returns the same cached AssetId - no re-decode");
    }

    void TestTextureNormalizedPathDoesNotDuplicateCache()
    {
        AssetManager manager = MakeTestManager();

        const auto plain = manager.LoadTexture("textures/checker_red.png");
        const auto dotted = manager.LoadTexture("./textures/checker_red.png");

        Check(plain.has_value() && dotted.has_value(), "Both path spellings load successfully");
        Check(*plain == *dotted, "Both path spellings resolve to the same cached texture asset");
    }

    void TestDistinctTexturesGetDistinctIds()
    {
        AssetManager manager = MakeTestManager();

        const auto red = manager.LoadTexture("textures/checker_red.png");
        const auto blue = manager.LoadTexture("textures/checker_blue.png");

        Check(red.has_value() && blue.has_value(), "Both distinct texture assets load");
        Check(!(*red == *blue), "Two different texture files receive different AssetIds");
    }

    void TestTextureRootTraversalIsRejected()
    {
        AssetManager manager = MakeTestManager();

        const auto escapeAttempt = manager.LoadTexture("../outside.png");
        Check(!escapeAttempt.has_value(), "A texture path that escapes the asset root is rejected, same as text/binary");
    }

    // --- M15: LoadMesh / GetMesh ---
    //
    // meshes/pyramid.obj has 6 triangular faces (18 total face-corner
    // references), 2 of which are exact (vertex_index, texcoord_index)
    // duplicates (the base quad's two triangles deliberately reuse
    // vt1/vt3) - so DecodeMeshOBJ must unify them down to exactly 16
    // unique vertices / 18 indices. The apex (v1, position (0,0.5,0))
    // is referenced by all 4 side faces with 4 DIFFERENT texcoord
    // indices, so it is deliberately NOT deduplicated - it appears as
    // 4 separate MeshVertexData entries, all sharing the same position.

    void TestMeshLoadSucceedsWithCorrectCounts()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadMesh("meshes/pyramid.obj");
        Check(id.has_value(), "A valid OBJ file loads successfully");
        Check(id->IsValid(), "The returned AssetId is valid");

        const MeshAsset& asset = manager.GetMesh(*id);
        Check(asset.vertices.size() == 16, "pyramid.obj's 18 face-corner references unify into exactly 16 vertices");
        Check(asset.indices.size() == 18, "pyramid.obj has exactly 18 indices (6 triangular faces * 3)");
    }

    void TestMeshLoadDedupOccurred()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadMesh("meshes/pyramid.obj");
        Check(id.has_value(), "pyramid.obj loads");
        const MeshAsset& asset = manager.GetMesh(*id);

        Check(asset.vertices.size() < asset.indices.size(),
              "Unique vertex count is strictly less than the raw index count - real dedup occurred, not a 1:1 passthrough");
    }

    void TestMeshLoadHasCorrectApexPosition()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadMesh("meshes/pyramid.obj");
        Check(id.has_value(), "pyramid.obj loads");
        const MeshAsset& asset = manager.GetMesh(*id);

        const AREngine::Core::Math::Vec3 apex(0.0f, 0.5f, 0.0f);
        std::size_t apexCount = 0;
        for (const MeshVertexData& vertex : asset.vertices)
        {
            if (vertex.position == apex) { ++apexCount; }
        }
        Check(apexCount == 4, "The apex position appears exactly 4 times (once per side face, each with a distinct UV - not deduplicated)");
    }

    void TestMeshLoadHasCorrectUV()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadMesh("meshes/pyramid.obj");
        Check(id.has_value(), "pyramid.obj loads");
        const MeshAsset& asset = manager.GetMesh(*id);

        // B0 = (-0.5,-0.5,-0.5), the base corner assigned vt1 = (0,0) -
        // the same corner/UV pairing ProceduralMesh::AppendFace uses.
        const AREngine::Core::Math::Vec3 b0(-0.5f, -0.5f, -0.5f);
        const AREngine::Core::Math::Vec2 expectedUv(0.0f, 0.0f);
        bool found = false;
        for (const MeshVertexData& vertex : asset.vertices)
        {
            if (vertex.position == b0 && vertex.uv == expectedUv) { found = true; break; }
        }
        Check(found, "A vertex with B0's position and its expected (0,0) UV exists in the decoded mesh");
    }

    void TestMeshLoadWithNoTexcoordsDefaultsUvToZero()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadMesh("meshes/triangle.obj");
        Check(id.has_value(), "triangle.obj (no vt records at all) loads successfully");
        const MeshAsset& asset = manager.GetMesh(*id);

        Check(asset.vertices.size() == 3, "triangle.obj has exactly 3 unique vertices");
        Check(asset.indices.size() == 3, "triangle.obj has exactly 3 indices (1 face * 3)");
        for (const MeshVertexData& vertex : asset.vertices)
        {
            Check(vertex.uv == AREngine::Core::Math::Vec2(0.0f, 0.0f),
                  "A face corner with no vt (texcoord_index == -1) defaults its UV to (0,0)");
        }
    }

    void TestMeshMissingFileFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto result = manager.LoadMesh("meshes/does_not_exist.obj");
        Check(!result.has_value(), "Loading a missing OBJ file returns std::nullopt, not a crash");
    }

    void TestMeshCorruptFileFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto result = manager.LoadMesh("meshes/corrupt.obj");
        Check(!result.has_value(), "Loading a file with an .obj extension but no parseable v/vt/f records returns std::nullopt, not a crash");
    }

    void TestMeshEmptyFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto result = manager.LoadMesh("meshes/empty_mesh.obj");
        Check(!result.has_value(), "Loading a syntactically valid OBJ with real vertices but zero faces returns std::nullopt");
    }

    void TestMeshInvalidIndexFailsPredictably()
    {
        AssetManager manager = MakeTestManager();

        const auto result = manager.LoadMesh("meshes/invalid_index.obj");
        Check(!result.has_value(), "Loading an OBJ whose face references an out-of-range vertex index returns std::nullopt, not a crash");
    }

    void TestMeshCachingReturnsSameIdentity()
    {
        AssetManager manager = MakeTestManager();

        const auto first = manager.LoadMesh("meshes/pyramid.obj");
        const auto second = manager.LoadMesh("meshes/pyramid.obj");

        Check(first.has_value() && second.has_value(), "Both loads succeed");
        Check(*first == *second, "Loading the same mesh twice returns the same cached AssetId - no re-parse");
    }

    void TestMeshNormalizedPathDoesNotDuplicateCache()
    {
        AssetManager manager = MakeTestManager();

        const auto plain = manager.LoadMesh("meshes/pyramid.obj");
        const auto dotted = manager.LoadMesh("./meshes/pyramid.obj");

        Check(plain.has_value() && dotted.has_value(), "Both path spellings load successfully");
        Check(*plain == *dotted, "Both path spellings resolve to the same cached mesh asset");
    }

    void TestDistinctMeshesGetDistinctIds()
    {
        AssetManager manager = MakeTestManager();

        const auto pyramid = manager.LoadMesh("meshes/pyramid.obj");
        const auto triangle = manager.LoadMesh("meshes/triangle.obj");

        Check(pyramid.has_value() && triangle.has_value(), "Both distinct mesh assets load");
        Check(!(*pyramid == *triangle), "Two different OBJ files receive different AssetIds");
    }

    void TestMeshRootTraversalIsRejected()
    {
        AssetManager manager = MakeTestManager();

        const auto escapeAttempt = manager.LoadMesh("../outside.obj");
        Check(!escapeAttempt.has_value(), "A mesh path that escapes the asset root is rejected, same as text/binary/texture");
    }

    void TestMeshAssetIdIsValidAfterLoad()
    {
        AssetManager manager = MakeTestManager();

        const auto id = manager.LoadMesh("meshes/pyramid.obj");
        Check(id.has_value(), "pyramid.obj loads");
        Check(manager.IsValid(*id), "AssetManager::IsValid recognizes a freshly-loaded mesh AssetId (checks the mesh map, not just text/binary/texture)");
    }
}

int main()
{
    TestManagerCreation();
    TestTextLoadSucceedsWithCorrectContents();
    TestBinaryLoadSucceedsWithCorrectBytes();
    TestMissingFileFailsPredictably();
    TestInvalidAssetId();
    TestDistinctAssetsGetDistinctIds();
    TestCachingReturnsSameIdentity();
    TestNormalizedPathDoesNotDuplicateCache();
    TestRootTraversalIsRejected();
    TestSamePathAsTextAndBinaryAreIndependent();

    TestTextureLoadSucceedsWithCorrectDimensions();
    TestTextureLoadDecodesPlausibleColorContent();
    TestTextureMissingFileFailsPredictably();
    TestTextureCorruptFileFailsPredictably();
    TestTextureCachingReturnsSameIdentity();
    TestTextureNormalizedPathDoesNotDuplicateCache();
    TestDistinctTexturesGetDistinctIds();
    TestTextureRootTraversalIsRejected();

    TestMeshLoadSucceedsWithCorrectCounts();
    TestMeshLoadDedupOccurred();
    TestMeshLoadHasCorrectApexPosition();
    TestMeshLoadHasCorrectUV();
    TestMeshLoadWithNoTexcoordsDefaultsUvToZero();
    TestMeshMissingFileFailsPredictably();
    TestMeshCorruptFileFailsPredictably();
    TestMeshEmptyFailsPredictably();
    TestMeshInvalidIndexFailsPredictably();
    TestMeshCachingReturnsSameIdentity();
    TestMeshNormalizedPathDoesNotDuplicateCache();
    TestDistinctMeshesGetDistinctIds();
    TestMeshRootTraversalIsRejected();
    TestMeshAssetIdIsValidAfterLoad();

    if (g_failureCount == 0)
    {
        std::printf("All Assets M6/M14/M15 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
