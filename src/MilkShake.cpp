#include <iostream>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

const int WIDTH = 800;
const int HEIGHT = 600;

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "MilkShake_RayTracing", nullptr, nullptr);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}