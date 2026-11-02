#pragma once

#include <Core/DA_API.h>
#include "ModelData.h"

#include <optional>
#include <string>

namespace DA {
    // Loads .gltf / .glb (triangle meshes, node hierarchy, instancing). Returns nullopt on failure (logged).
    // Not supported yet: Draco / meshopt compression, materials, skins, animation.
    DA_API std::optional<ModelData> Loadgltf(const std::string& path);
} // namespace DA
