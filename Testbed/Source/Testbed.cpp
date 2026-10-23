/**
 * @file Testbed.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#include <Core/Entry.h>

class Testbed final : public DA::Application {
public:
    Testbed() : Application({.Name = "DA Testbed", .Width = 1280, .Height = 720, .Resizable = true}) {}

protected:
    // clang-format off
    bool OnInit() override {
        DA_INFO("Testbed initialized.");
        return true;
    }
    void OnUpdate(float deltaSeconds) override {
        (void)deltaSeconds; 
    }
    void OnShutdown() override { 
        DA_INFO("Testbed shutting down."); 
    }
    // clang-format on
};

std::unique_ptr<DA::Application> DA::CreateApplication() {
    return std::make_unique<Testbed>();
}
