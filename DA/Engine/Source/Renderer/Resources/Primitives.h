#pragma once

#include <Core/DA_API.h>
#include "Vertex.h"

#include <cstdint>

namespace DA {
    // Unit-style cube centered at the origin (24 vertices, so every face has its own normal and UVs).
    DA_API MeshData MakeCube(float halfExtent = 0.5f);

    // Flat square in the XZ plane at y = 0, facing +Y. The texture repeats `uvTiles` times across it.
    DA_API MeshData MakePlane(float halfExtent = 5.0f, float uvTiles = 5.0f);

    MeshData MakeSphere(float radius = 0.5f, uint32_t sectors = 32, uint32_t stacks = 16);

    MeshData MakeCylinder(float radius = 0.5f, float height = 1.0f, uint32_t sectors = 32);

    MeshData MakeCone(float radius = 0.5f, float height = 1.0f, uint32_t sectors = 32);

    MeshData MakeCapsule(float radius = 0.5f, float height = 2.0f, uint32_t sectors = 32,
                         uint32_t hemisphereStacks = 8);
} // namespace DA
