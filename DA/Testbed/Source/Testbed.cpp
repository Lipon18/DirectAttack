/**
 * @file Testbed.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#include <Core/Entry.h>
#include <Core/Input/Input.h>
#include <Renderer/Resources/Primitives.h>
#include <Renderer/VulkanRenderer.h>
#include <Scene/Camera.h>
#include <Scene/FlyCameraController.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

class Testbed final : public DA::Application {
public:
    Testbed() : Application({.Name = "DA Testbed", .Width = 1280, .Height = 720, .Resizable = true}) {}

protected:
    // clang-format off
    bool OnInit() override {
        auto& renderer = GetEngine().GetRenderer();
        m_cube = renderer.CreateMesh(DA::MakeCube());
        m_plane = renderer.CreateMesh(DA::MakePlane(8.0f , 8.0f));
        if(!m_cube || !m_plane) return false;

        auto& input = GetEngine().GetInput();
        DA::FlyCameraController::BindDefaultControls(input);
        input.GetScheme().AddButton("ReleaseCursor", DA::Bind::KeyButton(DA::Key::Escape));

        m_camera.SetPosition({3.0f, 3.0f, 6.0f});
        m_camera.LookAt({0.0f, 0.0f, 0.0f});

        DA_INFO("Testbed initialized. Click to capture the mouse, WASD/QE to move, Shift to sprint, Esc to release.");
        return true;
    }
    void OnUpdate(float deltaSeconds) override {
        m_time += deltaSeconds;
        auto& renderer = GetEngine().GetRenderer();
        auto& input = GetEngine().GetInput();

        if(!input.IsCursorCaptured() && input.IsMousePressed(DA::MouseButton::Left)) input.SetCursorCaptured(true);
        if (input.IsButtonPressed("ReleaseCursor")) input.SetCursorCaptured(false);
        if(input.IsCursorCaptured()) m_fly.Update(m_camera, input, deltaSeconds);
        renderer.SetCamera(m_camera);

        // Floor, just below the cubes.
        renderer.Submit({m_plane.get(), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f))});

        // Center cube, spinning.
        glm::mat4 center = glm::rotate(glm::mat4(1.0f), m_time * 0.8f, glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f)));
        renderer.Submit({m_cube.get(), center});

        // Left cube, fixed position, slow spin.
        glm::mat4 left = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
        left = glm::rotate(left, m_time * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
        renderer.Submit({m_cube.get(), left});

        // Small cube orbiting through the center one: the occlusion test for the depth buffer.
        glm::mat4 orbit = glm::translate(glm::mat4(1.0f),
                                         glm::vec3(std::cos(m_time) * 0.9f, 0.2f, std::sin(m_time) * 0.9f));
        orbit = glm::scale(glm::rotate(orbit, m_time * 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.45f));
        renderer.Submit({m_cube.get(), orbit});
    }

    void OnShutdown() override { 
         // Release GPU resources while the engine's device and allocator still exist.
        m_cube.reset();
        m_plane.reset();
        DA_INFO("Testbed shutting down.");
    }
    // clang-format on

private:
    std::unique_ptr<DA::VulkanMesh> m_cube;
    std::unique_ptr<DA::VulkanMesh> m_plane;
    DA::Camera m_camera;
    DA::FlyCameraController m_fly;
    float m_time = 0.0f;
};

std::unique_ptr<DA::Application> DA::CreateApplication() {
    return std::make_unique<Testbed>();
}
