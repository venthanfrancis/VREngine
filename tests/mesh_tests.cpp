// M8H tests for AREngine::Rendering's generic, backend-independent mesh
// data (MeshData.hpp) and procedural mesh generators (ProceduralMesh.hpp).
// Zero Vulkan, zero GPU — built and run unconditionally, not gated
// behind ARENGINE_ENABLE_VULKAN, since none of this depends on Vulkan
// at all. See docs/ARCHITECTURE.md, "CPU Mesh Data Placement (M8H)".

#include "AREngine/Rendering/MeshData.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"

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

    using namespace AREngine::Rendering;

    // --- MeshData::IsValid() ---

    void TestEmptyMeshDataIsInvalid()
    {
        Check(!MeshData{}.IsValid(), "A default-constructed (empty) MeshData is invalid");
    }

    void TestMeshDataRejectsEmptyIndicesWithVertices()
    {
        MeshData mesh;
        mesh.vertices = {MeshVertex{}, MeshVertex{}};
        Check(!mesh.IsValid(), "Vertices present but no indices is invalid (not an indexed mesh)");
    }

    void TestMeshDataRejectsEmptyVerticesWithIndices()
    {
        MeshData mesh;
        mesh.indices = {0, 1, 2};
        Check(!mesh.IsValid(), "Indices present but no vertices is invalid");
    }

    void TestMeshDataRejectsOutOfRangeIndex()
    {
        MeshData mesh;
        mesh.vertices = {MeshVertex{}, MeshVertex{}, MeshVertex{}}; // 3 vertices: valid indices are 0..2
        mesh.indices = {0, 1, 3}; // 3 is out of range
        Check(!mesh.IsValid(), "An index equal to (or beyond) the vertex count is invalid");
    }

    void TestMeshDataAcceptsValidGeometry()
    {
        MeshData mesh;
        mesh.vertices = {MeshVertex{}, MeshVertex{}, MeshVertex{}};
        mesh.indices = {0, 1, 2};
        Check(mesh.IsValid(), "Non-empty vertices/indices with every index in range is valid");
    }

    // --- CreateQuadMesh() ---

    void TestQuadMeshIsValid()
    {
        Check(CreateQuadMesh().IsValid(), "CreateQuadMesh produces valid MeshData");
    }

    void TestQuadMeshVertexIndexCounts()
    {
        const MeshData quad = CreateQuadMesh();
        Check(quad.vertices.size() == 4, "A quad has exactly 4 vertices");
        Check(quad.indices.size() == 6, "A quad has exactly 6 indices (2 triangles)");
    }

    // --- CreateCubeMesh() ---

    void TestCubeMeshIsValid()
    {
        Check(CreateCubeMesh().IsValid(), "CreateCubeMesh produces valid MeshData");
    }

    void TestCubeMeshVertexIndexCounts()
    {
        const MeshData cube = CreateCubeMesh();
        // 6 faces x 4 vertices each (not 8 - see ProceduralMesh.hpp for
        // why corners aren't shared across faces: each face needs its
        // own independent UV mapping at every corner).
        Check(cube.vertices.size() == 24, "A cube has exactly 24 vertices (6 faces x 4, not shared)");
        // 6 faces x 2 triangles x 3 indices.
        Check(cube.indices.size() == 36, "A cube has exactly 36 indices (6 faces x 2 triangles)");
    }

    void TestCubeMeshIndicesInVertexRange()
    {
        const MeshData cube = CreateCubeMesh();
        bool allInRange = true;
        for (const std::uint32_t index : cube.indices)
        {
            if (index >= cube.vertices.size())
            {
                allInRange = false;
                break;
            }
        }
        Check(allInRange, "Every cube index refers to a real vertex");
    }

    void TestCubeMeshIsOneMeterCentered()
    {
        // Every axis must reach exactly -0.5 and +0.5 (1 meter, centered
        // on the origin - see docs/WORLD_CONVENTIONS.md) and never
        // exceed that range.
        const MeshData cube = CreateCubeMesh();
        float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f, minZ = 0.0f, maxZ = 0.0f;
        bool first = true;
        for (const MeshVertex& v : cube.vertices)
        {
            if (first)
            {
                minX = maxX = v.position.x;
                minY = maxY = v.position.y;
                minZ = maxZ = v.position.z;
                first = false;
                continue;
            }
            minX = v.position.x < minX ? v.position.x : minX;
            maxX = v.position.x > maxX ? v.position.x : maxX;
            minY = v.position.y < minY ? v.position.y : minY;
            maxY = v.position.y > maxY ? v.position.y : maxY;
            minZ = v.position.z < minZ ? v.position.z : minZ;
            maxZ = v.position.z > maxZ ? v.position.z : maxZ;
        }

        Check(minX == -0.5f && maxX == 0.5f, "Cube X extent is exactly [-0.5, 0.5] (1 meter)");
        Check(minY == -0.5f && maxY == 0.5f, "Cube Y extent is exactly [-0.5, 0.5] (1 meter)");
        Check(minZ == -0.5f && maxZ == 0.5f, "Cube Z extent is exactly [-0.5, 0.5] (1 meter)");
    }
}

int main()
{
    TestEmptyMeshDataIsInvalid();
    TestMeshDataRejectsEmptyIndicesWithVertices();
    TestMeshDataRejectsEmptyVerticesWithIndices();
    TestMeshDataRejectsOutOfRangeIndex();
    TestMeshDataAcceptsValidGeometry();

    TestQuadMeshIsValid();
    TestQuadMeshVertexIndexCounts();

    TestCubeMeshIsValid();
    TestCubeMeshVertexIndexCounts();
    TestCubeMeshIndicesInVertexRange();
    TestCubeMeshIsOneMeterCentered();

    if (g_failureCount == 0)
    {
        std::printf("All Mesh (pure-logic) M8H checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
