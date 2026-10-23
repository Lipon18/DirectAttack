#include "PCH.h"
#include "Application.h"
#include "Engine.h"
#include <assert.h>

namespace DA {
    Application::Application(ApplicationConfig config) : m_config(std::move(config)) {}

    Engine& Application::GetEngine() const {
        Engine* engine = Engine::Get();
        assert(engine && "Application is not attached to an Engine.");
        return *engine;
    }
} // namespace DA
