#include "PCH.h"
#include "Primitives.h"

namespace DA {
    namespace {
        void AddFace(MeshData& mesh, const glm::vec3& center, const glm::vec3& n, const glm::vec3& u,
                     const glm::vec3& v, float halfU, float halfV, const glm::vec3& color, float uvTiles) {
            const uint32_t base = static_cast<uint32_t>(mesh.Vertices.size());
            const glm::vec3 du = u * halfU;
            const glm::vec3 dv = v * halfV;

            mesh.Vertices.push_back({center - du - dv, n, {0.0f, uvTiles}, color});
            mesh.Vertices.push_back({center + du - dv, n, {uvTiles, uvTiles}, color});
            mesh.Vertices.push_back({center + du + dv, n, {uvTiles, 0.0f}, color});
            mesh.Vertices.push_back({center - du + dv, n, {0.0f, 0.0f}, color});

            for (uint32_t i : {0u, 1u, 2u, 2u, 3u, 0u})
                mesh.Indices.push_back(base + i);
        }

        void AddVertex(MeshData& mesh, const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv,
            const glm::vec3& color) {
            mesh.Vertices.push_back({position, normal, uv, color});
        }

        void AddTriangle(MeshData& mesh, uint32_t a, uint32_t b, uint32_t c) {
            mesh.Indices.push_back(a);
            mesh.Indices.push_back(b);
            mesh.Indices.push_back(c);
        }
    } // namespace
    MeshData MakeCube(float h) {
        MeshData mesh;
        const glm::vec3 X{1, 0, 0}, Y{0, 1, 0}, Z{0, 0, 1};
        //       center    normal  u    v   half  half  color                    tiles
        AddFace(mesh, Z * h, Z, X, Y, h, h, {1.0f, 0.3f, 0.3f}, 1.0f);    // +Z
        AddFace(mesh, -Z * h, -Z, -X, Y, h, h, {0.3f, 1.0f, 0.3f}, 1.0f); // -Z
        AddFace(mesh, X * h, X, -Z, Y, h, h, {0.3f, 0.4f, 1.0f}, 1.0f);   // +X
        AddFace(mesh, -X * h, -X, Z, Y, h, h, {1.0f, 1.0f, 0.3f}, 1.0f);  // -X
        AddFace(mesh, Y * h, Y, X, -Z, h, h, {1.0f, 0.3f, 1.0f}, 1.0f);   // +Y
        AddFace(mesh, -Y * h, -Y, X, Z, h, h, {0.3f, 1.0f, 1.0f}, 1.0f);  // -Y
        return mesh;
    }

    MeshData MakePlane(float halfExtent, float uvTiles) {
        MeshData mesh;
        AddFace(mesh, {0, 0, 0}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, halfExtent, halfExtent, {1.0f, 1.0f, 1.0f}, uvTiles);
        return mesh;
    }
} // namespace DA
