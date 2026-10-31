#pragma once

#include <Core/DA_API.h>

#include <glm/glm.hpp>

namespace DA {
    // Right-handed, Y up. Yaw 0 looks along +X, yaw -90 looks along -Z. Angles are in degrees.
    class DA_API Camera {
    public:
        void SetPosition(const glm::vec3& position) { m_position = position; }
        const glm::vec3& GetPosition() const { return m_position; }

        void SetRotation(float yawDegrees, float pitchDegrees);
        void Rotate(float deltaYawDegrees, float deltaPitchDegrees);
        void LookAt(const glm::vec3& target);

        float GetYaw() const { return m_yaw; }
        float GetPitch() const { return m_pitch; }

        void SetFieldOfView(float degrees) { m_fov = degrees; }
        void SetClipPlanes(float nearPlane, float farPlane) {
            m_near = nearPlane;
            m_far = farPlane;
        }
        float GetFieldOfView() const { return m_fov; }
        float GetNearPlane() const { return m_near; }
        float GetFarPlane() const { return m_far; }

        glm::vec3 GetForward() const;
        glm::vec3 GetRight() const;
        glm::vec3 GetUp() const;

        glm::mat4 GetViewMatrix() const;
        // Vulkan-style: depth 0..1 and Y pointing down, so the matrix is ready to use as-is.
        glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    private:
        glm::vec3 m_position{0.0f};
        float m_yaw = -90.0f;
        float m_pitch = 0.0f;
        float m_fov = 60.0f;
        float m_near = 0.1f;
        float m_far = 1000.0f;
    };
} // namespace DA
