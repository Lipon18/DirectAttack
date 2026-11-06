#include <Core/Entry.h>
#include <Core/Input/Input.h>
#include <Core/Paths.h>
#include <Renderer/Resources/Primitives.h>
#include <Renderer/VulkanRenderer.h>
#include <Scene/Camera.h>
#include <Scene/FlyCameraController.h>
#include <Renderer/Resources/gltfLoader.h>
#include <Renderer/Resources/VulkanModel.h>
#include <Renderer/Resources/ModelAsset.h>
#include <Renderer/Resources/SkyboxMaterial.h>

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
        DA::CubeMapFaces faces;
        const auto sky = [](const char* f){return DA::Paths::ResolveEngine(std::string("Textures/Skybox/") + f);};
        faces.Paths = {sky("right.png"), sky("left.png"), sky("top.png"), sky("bottom.png"), sky("front.png"), sky("back.png")};
        if (auto cube = renderer.CreateCubeMap(faces)) {
            renderer.SetSkybox(std::make_unique<DA::SkyboxMaterial>(std::move(cube)));
        }
        m_helmet = renderer.LoadModelAsync(DA::Paths::ResolveEngine("Models/DamagedHelmet.gltf"));
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

        /*m_logTimer += deltaSeconds;
        if (m_logTimer >= 1.0f) {
            m_logTimer -= 1.0f;
            DA_INFO("Renderer Stats | Drawn: {} | Culled: {}", renderer.GetLastDrawn(), renderer.GetLastCulled());
        }*/

        if(!input.IsCursorCaptured() && input.IsMousePressed(DA::MouseButton::Left)) input.SetCursorCaptured(true);
        if (input.IsButtonPressed("ReleaseCursor")) input.SetCursorCaptured(false);
        if(input.IsCursorCaptured()) m_fly.Update(m_camera, input, deltaSeconds);
        renderer.SetCamera(m_camera);

        // Helmet
        if (m_helmet && m_helmet->IsReady()) {
            if (!m_fitReady) {
                m_modelFit = DA::FitToSize(m_helmet->GetBounds(), 2.0f);
                m_fitReady = true;
            }
            glm::mat4 helmetTransform  = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, -3.0f));
            helmetTransform = helmetTransform * glm::rotate(glm::mat4(1.0f), m_time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            helmetTransform  = helmetTransform  * m_modelFit;
            //root = glm::rotate(root, m_time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)) * m_modelFit;
            renderer.Submit(*m_helmet->Get(), /*root*/ helmetTransform );
        }

        // Floor, just below the cubes.
        glm::mat4 floorTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        renderer.Submit({m_plane.get(), floorTransform});

        // Cube
        glm::mat4 cubeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
        cubeTransform = glm::rotate(cubeTransform, m_time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        renderer.Submit({m_cube.get(), cubeTransform});
    }

    void OnShutdown() override { 
         // Release GPU resources while the engine's device and allocator still exist.
        m_cube.reset();
        m_plane.reset();
        m_helmet.reset();
        DA_INFO("Testbed shutting down.");
    }
    // clang-format on

private:
    std::unique_ptr<DA::VulkanMesh> m_cube;
    std::unique_ptr<DA::VulkanMesh> m_plane;
    std::shared_ptr<DA::ModelAsset> m_helmet;
    DA::Camera m_camera;
    DA::FlyCameraController m_fly;
    float m_time = 0.0f;
    glm::mat4 m_modelFit{1.0f};
    bool m_fitReady = false;

    std::shared_ptr<DA::VulkanImage> m_cubeTexture;
    std::shared_ptr<DA::VulkanImage> m_floorTexture;

    // float m_logTimer = 0.0f;
};

std::unique_ptr<DA::Application> DA::CreateApplication() {
    return std::make_unique<Testbed>();
}
