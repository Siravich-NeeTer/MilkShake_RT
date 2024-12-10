#pragma once

#include <glfw/glfw3.h>
#include <iostream>
#include <cstring>

namespace Input
{
	extern float mouseX;
	extern float mouseY;
	extern float scroll;
	extern bool keyPressed[GLFW_KEY_LAST];
	extern bool keyBeginPressed[GLFW_KEY_LAST];
	extern bool keyEndPressed[GLFW_KEY_LAST];

	bool IsKeyPressed(const int& key);
	bool IsKeyBeginPressed(const int& key);
	bool IsKeyEndPressed(const int& key);

	void EndFrame();

	void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
	void CursorCallBack(GLFWwindow* window, double xPos, double yPos);
	void MouseCallBack(GLFWwindow* window, int key, int action, int mods);
	void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};