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
#include <Renderer/Resources/PrimitiveLibrary.h>
#include <Renderer/Resources/VulkanMaterial.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>

class Testbed final : public DA::Application {
public:
    Testbed() : Application({.Name = "DA Testbed", .Width = 1280, .Height = 720, .Resizable = true}) {}

protected:
    // clang-format off
    bool OnInit() override {
        auto& renderer = GetEngine().GetRenderer();
        /*m_cube = renderer.CreateMesh(DA::MakeCube());*/
        m_plane = renderer.CreateMesh(DA::MakePlane(8.0f , 8.0f));
        if(!m_primitives.Load(renderer)) return false;
        DA::CubeMapFaces faces;
        const auto sky = [](const char* f){return DA::Paths::ResolveEngine(std::string("Textures/Skybox/") + f);};
        faces.Paths = {sky("right.png"), sky("left.png"), sky("top.png"), sky("bottom.png"), sky("front.png"), sky("back.png")};
        if (auto cube = renderer.CreateCubeMap(faces)) {
            renderer.SetSkybox(std::make_unique<DA::SkyboxMaterial>(std::move(cube)));
        }
        m_helmet = renderer.LoadModelAsync(DA::Paths::ResolveEngine("Models/DamagedHelmet.gltf"));
        if(/*!m_cube || */!m_plane) return false;

        DA::Environment env;
        env.Ambient = {0.15f, 0.15f, 0.20f};
        env.Sun.Intensity = 0.5f;
        env.Fog = {.Enabled = true, .Color = {0.62f, 0.74f, 0.9f}, .Start = 25.0f, .End = 90.0f};
        renderer.SetEnvironment(env);

        DA::MaterialParams glass;
        glass.BaseColor = {0.5f, 0.8f, 1.0f, 0.35f};
        std::array<std::shared_ptr<DA::VulkanImage>, DA::VulkanMaterial::SlotCount> none{}; // all slots fall back to white
        m_glass = renderer.CreateMaterial(glass, none, /*doubleSided*/ false, /*blend*/ true);

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

        // Two glass cubes in front of the helmet. Submit order is deliberately "wrong": the sort fixes it.
        const glm::mat4 g1 = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.3f, 1.0f)), glm::vec3(1.5f));
        const glm::mat4 g2 = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(2.2f, 0.3f, 1.6f)), glm::vec3(1.2f));
        renderer.Submit({m_cube.get(), g1, m_glass.get()});
        renderer.Submit({m_cube.get(), g2, m_glass.get()});

        // Floor, just below the cubes.
        glm::mat4 floorTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        renderer.Submit({m_plane.get(), floorTransform});

        // Cube
        /*if (m_primitives.IsReady(DA::EPrimitive::E_CUBE)) {
            auto cube = m_primitives.Get(DA::EPrimitive::E_CUBE);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
            transform = glm::rotate(transform, m_time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            renderer.Submit(*cube->Get(), transform);
        }*/
        /*glm::mat4 cubeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
        cubeTransform = glm::rotate(cubeTransform, m_time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        renderer.Submit({m_cube.get(), cubeTransform});*/

        // Submit an orbiting Point Light around the cube
        DA::PointLight light;
        light.Position = glm::vec3(std::sin(m_time * 1.5f)* 2.5f, 1.5f, std::cos(m_time * 1.5f) * 2.5f);
        light.Color = glm::vec3(1.0f, 0.8f, 0.2f);
        light.Intensity = 5.0f;
        light.Radius = 8.0f;
        renderer.SubmitLight(light);
    }

    void OnShutdown() override { 
         // Release GPU resources while the engine's device and allocator still exist.
        //m_cube.reset();
        m_primitives.Unload();
        m_plane.reset();
        m_helmet.reset();
        m_glass.reset();
        DA_INFO("Testbed shutting down.");
    }
    // clang-format on

private:
    std::unique_ptr<DA::VulkanMesh> m_cube;
    std::unique_ptr<DA::VulkanMesh> m_plane;
    std::shared_ptr<DA::ModelAsset> m_helmet;
    DA::PrimitiveLibrary m_primitives;
    DA::Camera m_camera;
    DA::FlyCameraController m_fly;
    float m_time = 0.0f;
    glm::mat4 m_modelFit{1.0f};
    bool m_fitReady = false;

    std::shared_ptr<DA::VulkanImage> m_cubeTexture;
    std::shared_ptr<DA::VulkanImage> m_floorTexture;
    std::unique_ptr<DA::VulkanMaterial> m_glass;
    // float m_logTimer = 0.0f;
};

std::unique_ptr<DA::Application> DA::CreateApplication() {
    return std::make_unique<Testbed>();
}
