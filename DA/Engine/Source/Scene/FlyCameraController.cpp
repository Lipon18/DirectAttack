#include "PCH.h"
#include "FlyCameraController.h"
#include "Camera.h"
#include <Core/Input/Input.h>

namespace DA {
    void FlyCameraController::BindDefaultControls(Input& input) {
        InputScheme& s = input.GetScheme();
        s.AddAxis("MoveForward", Bind::KeyAxis(Key::S, Key::W));
        s.AddAxis("MoveRight", Bind::KeyAxis(Key::A, Key::D));
        s.AddAxis("MoveUp", Bind::KeyAxis(Key::Q, Key::E));
        s.AddAxis("LookX", Bind::MouseDelta(MouseAxisId::X));
        s.AddAxis("LookY", Bind::MouseDelta(MouseAxisId::Y));
        s.AddButton("Sprint", Bind::KeyButton(Key::LeftShift));
    }

    void FlyCameraController::Update(Camera& camera, const Input& input, float deltaSeconds) const {
        // Look: mouse right turns right (yaw up); mouse down looks down (screen Y points down, pitch up is positive).
        camera.Rotate(input.GetAxis("LookX") * Settings.LookSensitivity,
                      -input.GetAxis("LookY") * Settings.LookSensitivity);

        glm::vec3 direction = camera.GetForward() * input.GetAxis("MoveForward") +
                              camera.GetRight() * input.GetAxis("MoveRight") +
                              glm::vec3(0.0f, 1.0f, 0.0f) * input.GetAxis("MoveUp");
        if (glm::dot(direction, direction) > 1.0f) direction = glm::normalize(direction); // no faster diagonals

        float speed = Settings.MoveSpeed;
        if (input.IsButtonDown("Sprint")) speed *= Settings.SprintMultiplier;

        camera.SetPosition(camera.GetPosition() + direction * speed * deltaSeconds);
    }
} // namespace DA