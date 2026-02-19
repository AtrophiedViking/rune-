#pragma once
#include <GLFW/glfw3.h>
struct State;
//IO CallBacks
void mouseCallback(GLFWwindow* window, double xpos, double ypos);

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

void charCallback(GLFWwindow* window, unsigned int codepoint);

void processInput(State* state);