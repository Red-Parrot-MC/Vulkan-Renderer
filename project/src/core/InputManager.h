#pragma once
#include "structs.h"
#include <iostream>

class InputManager
{
public:
	static bool GetKey(int key)
	{
		if (window)
			return glfwGetKey(window, key);
		return false;
	}

	// If key was down this frame
	static bool GetKeyDown(int key)
	{
		for (int downKey : keysDownThisFrame)
		{
			if (key == downKey) return true;
		}
		return false;
	}

	static void GetCursorPos(double* xpos, double* ypos)
	{
		if (window)
			glfwGetCursorPos(window, xpos, ypos);
	}

	static int GetMouseButton(int button)
	{
		if (window)
			return glfwGetMouseButton(window, button);
		return 0;
	}

	static void SetWindow(GLFWwindow* glfwWindow)
	{
		window = glfwWindow;
		glfwSetKeyCallback(window, KeyDown);
	}

	static void Update()
	{
		keysDownThisFrame.clear();
	}

private:
	static GLFWwindow* window;
	static std::vector<int> keysDownThisFrame;

	static void KeyDown(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if (action == GLFW_PRESS)
			keysDownThisFrame.push_back(key);
	}
};