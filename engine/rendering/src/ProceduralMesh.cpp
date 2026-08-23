#include "AREngine/Rendering/ProceduralMesh.hpp"

namespace AREngine::Rendering
{
    namespace
    {
        constexpr Core::Math::Vec3 kWhite(1.0f, 1.0f, 1.0f);

        // Appends one quad face (4 vertices, 6 indices) to `mesh`, given
        // its 4 corners already in counter-clockwise-from-outside order.
        // UVs always map corner 0..3 to (0,0)/(1,0)/(1,1)/(0,1) - the
        // same corner-to-UV order CreateQuadMesh uses on its own, so
        // every face maps the whole texture once, uncropped.
        void AppendFace(MeshData& mesh,
                         const Core::Math::Vec3& c0, const Core::Math::Vec3& c1,
                         const Core::Math::Vec3& c2, const Core::Math::Vec3& c3)
        {
            const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());

            mesh.vertices.push_back({c0, kWhite, {0.0f, 0.0f}});
            mesh.vertices.push_back({c1, kWhite, {1.0f, 0.0f}});
            mesh.vertices.push_back({c2, kWhite, {1.0f, 1.0f}});
            mesh.vertices.push_back({c3, kWhite, {0.0f, 1.0f}});

            mesh.indices.push_back(base + 0);
            mesh.indices.push_back(base + 1);
            mesh.indices.push_back(base + 2);
            mesh.indices.push_back(base + 2);
            mesh.indices.push_back(base + 3);
            mesh.indices.push_back(base + 0);
        }
    }

    MeshData CreateQuadMesh()
    {
        MeshData mesh;
        AppendFace(mesh,
            Core::Math::Vec3(-0.5f, -0.5f, 0.0f), Core::Math::Vec3(0.5f, -0.5f, 0.0f),
            Core::Math::Vec3(0.5f, 0.5f, 0.0f), Core::Math::Vec3(-0.5f, 0.5f, 0.0f));
        return mesh;
    }

    MeshData CreateCubeMesh()
    {
        using Core::Math::Vec3;

        MeshData mesh;
        mesh.vertices.reserve(24);
        mesh.indices.reserve(36);

        // +Z (front)
        AppendFace(mesh, Vec3(-0.5f, -0.5f, 0.5f), Vec3(0.5f, -0.5f, 0.5f), Vec3(0.5f, 0.5f, 0.5f), Vec3(-0.5f, 0.5f, 0.5f));
        // -Z (back)
        AppendFace(mesh, Vec3(0.5f, -0.5f, -0.5f), Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, 0.5f, -0.5f), Vec3(0.5f, 0.5f, -0.5f));
        // +X (right)
        AppendFace(mesh, Vec3(0.5f, -0.5f, 0.5f), Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, -0.5f), Vec3(0.5f, 0.5f, 0.5f));
        // -X (left)
        AppendFace(mesh, Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, -0.5f, 0.5f), Vec3(-0.5f, 0.5f, 0.5f), Vec3(-0.5f, 0.5f, -0.5f));
        // +Y (top)
        AppendFace(mesh, Vec3(-0.5f, 0.5f, 0.5f), Vec3(0.5f, 0.5f, 0.5f), Vec3(0.5f, 0.5f, -0.5f), Vec3(-0.5f, 0.5f, -0.5f));
        // -Y (bottom)
        AppendFace(mesh, Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, 0.5f), Vec3(-0.5f, -0.5f, 0.5f));

        return mesh;
    }
}
