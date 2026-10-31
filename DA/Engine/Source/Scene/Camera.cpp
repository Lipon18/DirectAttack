#include "PCH.h"
#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace DA {
    namespace {
        constexpr float MaxPitch = 89.0f; // straight up or down would flip the view
    }

    void Camera::SetRotation(float yawDegrees, float pitchDegrees) {
        m_yaw = yawDegrees;
        m_pitch = std::clamp(pitchDegrees, -MaxPitch, MaxPitch);
    }
    void Camera::Rotate(float deltaYawDegrees, float deltaPitchDegrees) {
        SetRotation(m_yaw + deltaYawDegrees, m_pitch + deltaPitchDegrees);
    }

    void Camera::LookAt(const glm::vec3& target) {
        const glm::vec3 d = glm::normalize(target - m_position);
        SetRotation(glm::degrees(std::atan2(d.z, d.x)), glm::degrees(std::asin(d.y)));
    }

    glm::vec3 Camera::GetForward() const {
        const float yaw = glm::radians(m_yaw);
        const float pitch = glm::radians(m_pitch);
        return glm::normalize(
            glm::vec3(std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw)));
    }

    glm::vec3 Camera::GetRight() const {
        return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    glm::vec3 Camera::GetUp() const {
        return glm::normalize(glm::cross(GetRight(), GetForward()));
    }

    glm::mat4 Camera::GetViewMatrix() const {
        return glm::lookAt(m_position, m_position + GetForward(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
        glm::mat4 projection = glm::perspective(glm::radians(m_fov), aspectRatio, m_near, m_far);
        projection[1][1] *= -1.0f; // Vulkan clip space has Y pointing down
        return projection;
    }
} // namespace DA
