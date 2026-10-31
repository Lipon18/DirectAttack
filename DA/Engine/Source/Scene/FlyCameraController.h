#pragma once

#include <Core/DA_API.h>

namespace DA {
    class Camera;
    class Input;

    struct FlyCameraSettings {
        float MoveSpeed = 4.0f; // units per second
        float SprintMultiplier = 3.0f;
        float LookSensitivity = 0.1f; // degrees per mouse pixel
    };

    class DA_API FlyCameraController {
    public:
        // Registers the axes and actions this controller reads: MoveForward, MoveRight, MoveUp, LookX, LookY, Sprint.
        static void BindDefaultControls(Input& input);

        void Update(Camera& camera, const Input& input, float deltaSeconds) const;

        FlyCameraSettings Settings;
    };
} // namespace DA