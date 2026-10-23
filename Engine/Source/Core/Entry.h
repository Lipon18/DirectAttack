/**
 * @file Entry.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "Engine.h"
#include "Application.h"
#include "Log.h"

#include <memory>

namespace DA {
    // Implemented by the game.
    std::unique_ptr<Application> CreateApplication();
}

int main() {
    DA::Log::Initialize();

    int result = 0;

    {
        DA::Engine engine;

        auto application = DA::CreateApplication();
        if (!application) {
            DA_FATAL("CreateApplication() returned null.");
            result - 1;
        } else if (!engine.Initialize(std::move(application))) {
            DA_FATAL("Engine initialization failed.");
            return -1;
        } else {
            result = engine.Run();
        }
    }

    DA::Log::Shutdown();

    return result;
}
