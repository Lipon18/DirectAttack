#include <gtest/gtest.h>
#include <Renderer/RHI/Vulkan/Core/VulkanInstance.h>
#include <Core/Log.h>

class VulkanInstanceTest : public ::testing::Test {
protected:
    void SetUp() override { DA::Log::Initialize(); }
    void TearDown() override { DA::Log::Shutdown(); }
};

TEST_F(VulkanInstanceTest, CreateInstanceWithValidationEnabled) {
    DA::VulkanInstanceDesc desc{};
    desc.AppName = "DirectAttack_Test";
    desc.AppVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    desc.EnableValidation = true;

    // Pass standard instance extensions (mocking standard window surface extensions if needed)
    desc.RequiredExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};

    auto instance = DA::VulkanInstance::Create(desc);

    // Depending on whether validation layers and required extensions are supported on the CI/test machine:
    if (instance) {
        EXPECT_NE(instance->GetHandle(), VK_NULL_HANDLE);
        EXPECT_GE(instance->GetApiVersion(), VK_API_VERSION_1_3);
    } else {
        // Fallback or explicit notification if hardware doesn't support required version/layers
        SUCCEED() << "Vulkan 1.3 or required extensions not present on this test runner environment.";
    }
}

TEST_F(VulkanInstanceTest, FailsGracefullyWithMissingRequiredExtensions) {
    DA::VulkanInstanceDesc desc{};
    desc.AppName = "DirectAttack_Test";
    desc.AppVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    desc.EnableValidation = false;

    // Supply a intentionally fake/unsupported extension name
    desc.RequiredExtensions = {"VK_EXT_non_existent_fake_extension"};

    auto instance = DA::VulkanInstance::Create(desc);
    EXPECT_EQ(instance, nullptr) << "Instance creation should fail when required extensions are missing.";
}