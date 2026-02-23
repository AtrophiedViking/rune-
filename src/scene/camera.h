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
	glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);  // Start at world origin
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);        // Y-axis as world up
	float yaw = 0.0f;                                 // Look along negative Z-axis (OpenGL convention)
	float pitch = 0.0f;
	float zoom = (85.0f);

	float movementSpeed = 1;
	float mouseSensitivity = 0.1f;

	bool lookMode;

	void updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    right = glm::normalize(glm::cross(front, worldUp));
    up    = glm::normalize(glm::cross(right, front));
}


	glm::mat4 getViewMatrix() const {
		return glm::lookAt(position, position + front, up);
	}
	glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane, float farPlane) const {
		return glm::perspective(glm::radians(zoom), aspectRatio, nearPlane, farPlane);
	};

	glm::vec3 getPosition() const { return position; }
	glm::vec3 getFront() const { return front; }
	float getZoom() const { return zoom; }

	void processKeyboard(CameraMovement direction, float deltaTime) {
		float velocity = movementSpeed * deltaTime;

		switch (direction) {
		case CameraMovement::FORWARD:
			position += front * velocity;
			break;
		case CameraMovement::BACKWARD:
			position -= front * velocity;
			break;
		case CameraMovement::LEFT:
			position -= right * velocity;
			break;
		case CameraMovement::RIGHT:
			position += right * velocity;
			break;
		case CameraMovement::UP:
			position += worldUp * velocity; 
			break;							
		case CameraMovement::DOWN:			
			position -= worldUp * velocity; 
			break;
		}
	}

	void processMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
		xOffset *= mouseSensitivity;
		yOffset *= mouseSensitivity;

		yaw += xOffset;
		pitch += yOffset;

		// Constrain pitch to avoid flipping
		if (constrainPitch) {
			pitch = std::clamp(pitch, -89.0f, 89.0f);
		}

		// Update camera vectors based on updated Euler angles
		if (lookMode) {
			updateCameraVectors();
		};
	}
};