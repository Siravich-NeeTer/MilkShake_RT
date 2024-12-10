#include "Input.h"

namespace Input
{
	float mouseX;
	float mouseY;
	float scroll;
	bool keyPressed[GLFW_KEY_LAST];
	bool keyBeginPressed[GLFW_KEY_LAST];
	bool keyEndPressed[GLFW_KEY_LAST];

	bool IsKeyPressed(const int& key)
	{
		return keyPressed[key];
	}

	bool IsKeyBeginPressed(const int& key)
	{
		return keyBeginPressed[key];
	}

	bool IsKeyEndPressed(const int& key)
	{
		return keyEndPressed[key];
	}

	void EndFrame()
	{
		for (int idx = 0; idx < GLFW_KEY_LAST; idx++)
		{
			keyBeginPressed[idx] = false;
			keyEndPressed[idx] = false;
		}
		scroll = 0.0f;
	}

	void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		// In case of : FN + F1 to FN + F12
		if (key < 0 || key >= GLFW_KEY_LAST) { return; }

		if (action == GLFW_PRESS)
		{
			keyBeginPressed[key] = true;
			keyPressed[key] = true;
		}
		else if (action == GLFW_RELEASE)
		{
			if (keyPressed[key])
				keyEndPressed[key] = true;

			keyPressed[key] = false;
			keyBeginPressed[key] = false;
		}

		if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
	}

	void CursorCallBack(GLFWwindow* window, double xPos, double yPos)
	{
		mouseX = (float)xPos;
		mouseY = (float)yPos;
	}

	void MouseCallBack(GLFWwindow* window, int key, int action, int mods)
	{
		if (action == GLFW_PRESS)
		{
			keyPressed[key] = true;
			keyBeginPressed[key] = true;
		}
		else if (action == GLFW_RELEASE)
		{
			if (keyPressed[key])
				keyEndPressed[key] = true;

			keyPressed[key] = false;
			keyBeginPressed[key] = false;
		}
	}
	void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
	{
		scroll = yoffset;
	}
}
