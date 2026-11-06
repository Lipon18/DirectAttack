#pragma once

#include "Core/DA_API.h"
#include "ModelData.h"
#include "VulkanModel.h"

#include <atomic>
#include <memory>
#include <string>

namespace DA {
    class VulkanRenderer;

    // Handle to a model that may still be loading. Query from the main thread.
    class DA_API ModelAsset {
    public:
        enum class State {
            Loading,
            Ready,
            Failed
        };

        ModelAsset(const ModelAsset&) = delete;
        ModelAsset& operator=(const ModelAsset&) = delete;

        State GetState() const { return m_state.load(std::memory_order_acquire); }
        bool IsReady() const { return GetState() == State::Ready; }
        bool IsFailed() const { return GetState() == State::Failed; }

        // Null until Ready.
        const VulkanModel* Get() const { return IsReady() ? m_model.get() : nullptr; }
        const AABB& GetBounds() const { return m_bounds; } // valid once Ready
        const std::string& GetName() const { return m_name; }

    private:
        friend class VulkanRenderer;
        ModelAsset() = default;

        std::atomic<State> m_state{State::Loading};
        std::unique_ptr<VulkanModel> m_model;
        AABB m_bounds;
        std::string m_name;
    };
} // namespace DA
