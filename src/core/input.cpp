#include "core/input.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "gui/gui.h"
#include "core/state.h"

//IO CallBacks
void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
	// First give ImGui the event
	ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

	// Then your camera logic
	auto state = static_cast<State*>(glfwGetWindowUserPointer(window));

	static bool firstMouse = true;
	static float lastX = 0.0f, lastY = 0.0f;

	if (firstMouse) {
		lastX = (float)xpos;
		lastY = (float)ypos;
		firstMouse = false;
	}

	float xoffset = (float)xpos - lastX;
	float yoffset = lastY - (float)ypos;

	lastX = (float)xpos;
	lastY = (float)ypos;

	if (state->scene->camera->lookMode)
		state->scene->camera->processMouseMovement(xoffset, yoffset, false);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	// First ImGui
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	// Then your logic
	auto state = static_cast<State*>(glfwGetWindowUserPointer(window));

	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			state->scene->camera->lookMode = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else if (action == GLFW_RELEASE) {
			state->scene->camera->lookMode = false;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}
void charCallback(GLFWwindow* window, unsigned int codepoint) {
	ImGui_ImplGlfw_CharCallback(window, codepoint);
}
void processInput(State* state) {
	// Calculate delta time
	static float lastFrame = 0.0f;
	float currentFrame = (float)glfwGetTime();
	float deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	ImGuiIO& io = ImGui::GetIO();

	if (!state->scene->camera->lookMode) {

	}
	else {

		// ⭐ CAMERA MOVEMENT MODE ⭐
		// lookMode = true → camera moves with WASD, space, ctrl

		if (glfwGetKey(state->window.handle, GLFW_KEY_W) == GLFW_PRESS)
			state->scene->camera->processKeyboard(CameraMovement::FORWARD, deltaTime);
		if (glfwGetKey(state->window.handle, GLFW_KEY_S) == GLFW_PRESS)
			state->scene->camera->processKeyboard(CameraMovement::BACKWARD, deltaTime);
		if (glfwGetKey(state->window.handle, GLFW_KEY_A) == GLFW_PRESS)
			state->scene->camera->processKeyboard(CameraMovement::LEFT, deltaTime);
		if (glfwGetKey(state->window.handle, GLFW_KEY_D) == GLFW_PRESS)
			state->scene->camera->processKeyboard(CameraMovement::RIGHT, deltaTime);
		if (glfwGetKey(state->window.handle, GLFW_KEY_SPACE) == GLFW_PRESS)
			state->scene->camera->processKeyboard(CameraMovement::UP, deltaTime);
		if (glfwGetKey(state->window.handle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			state->scene->camera->processKeyboard(CameraMovement::DOWN, deltaTime);
	}

	// Toggle Mouse Look Mode
	static bool previous = false;
	bool current = glfwGetKey(state->window.handle, GLFW_KEY_ESCAPE) == GLFW_PRESS;
	if (current && !previous) {
		state->scene->camera->lookMode = !state->scene->camera->lookMode;
		glfwSetInputMode(
			state->window.handle,
			GLFW_CURSOR,
			state->scene->camera->lookMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
		);
	}
	previous = current;
}
