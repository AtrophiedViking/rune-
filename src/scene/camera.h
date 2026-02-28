#pragma once
#include "core/math.h"


enum CameraMovement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};
struct Camera {
    // Right-handed, -Z forward (PBR standard)
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;   // -Z forward
    float pitch = 0.0f;
    float zoom = 100.0f;

    float viewDistance = 100.0f;
    float nearClip = 0.1f;
    float movementSpeed = 1.5f;
    float mouseSensitivity = 0.2f;

    bool lookMode = true;

    // ---------------------------------------------------------
    // FIXED: Euler → front conversion for right-handed -Z forward
    // ---------------------------------------------------------
    void updateCameraVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        front = glm::normalize(newFront);

        // Recompute right and up vectors
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }

    // ---------------------------------------------------------
    // FIXED: Right-handed view matrix (glm::lookAt is correct)
    // ---------------------------------------------------------
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    // ---------------------------------------------------------
    // FIXED: Vulkan projection with Y-flip
    // ---------------------------------------------------------
    glm::mat4 getProjectionMatrix(float aspect, float nearPlane, float farPlane) const {
        glm::mat4 proj = glm::perspective(glm::radians(zoom), aspect, nearPlane, farPlane);
        proj[1][1] *= -1.0f; // Vulkan Y flip
        return proj;
    }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    float getZoom() const { return zoom; }

    // ---------------------------------------------------------
    // MOVEMENT (unchanged)
    // ---------------------------------------------------------
    void processKeyboard(CameraMovement direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;

        switch (direction) {
        case FORWARD:  position += front * velocity; break;
        case BACKWARD: position -= front * velocity; break;
        case LEFT:     position -= right * velocity; break;
        case RIGHT:    position += right * velocity; break;
        case UP:       position += worldUp * velocity; break;
        case DOWN:     position -= worldUp * velocity; break;
        }
    }

    // ---------------------------------------------------------
    // MOUSE LOOK (unchanged except for corrected yaw default)
    // ---------------------------------------------------------
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
        xOffset *= mouseSensitivity;
        yOffset *= mouseSensitivity;

        yaw += xOffset;
        pitch += yOffset;

        if (constrainPitch)
            pitch = std::clamp(pitch, -89.0f, 89.0f);

        if (lookMode)
            updateCameraVectors();
    }
};
