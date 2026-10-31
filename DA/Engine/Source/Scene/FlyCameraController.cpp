#include "PCH.h"
#include "FlyCameraController.h"
#include "Camera.h"
#include <Core/Input/Input.h>

namespace DA {
    void FlyCameraController::BindDefaultControls(Input& input) {
        input.BindAxis("MoveForward", {AxisBinding::FromKey(Key::W, 1.0f), AxisBinding::FromKey(Key::S, -1.0f)});
        input.BindAxis("MoveRight", {AxisBinding::FromKey(Key::D, 1.0f), AxisBinding::FromKey(Key::A, -1.0f)});
        input.BindAxis("MoveUp", {AxisBinding::FromKey(Key::E, 1.0f), AxisBinding::FromKey(Key::Q, -1.0f)});
        input.BindAxis("LookX", {AxisBinding::MouseX()});
        input.BindAxis("LookY", {AxisBinding::MouseY()});
        input.BindAction("Sprint", {Key::LeftShift});
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
        if (input.IsActionDown("Sprint")) speed *= Settings.SprintMultiplier;

        camera.SetPosition(camera.GetPosition() + direction * speed * deltaSeconds);
    }
} // namespace DA