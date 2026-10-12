#include "Window.h"
#include "InputManager.h"
#include "glfwInclude.h"

void Window::Create(int width, int height, const char* title)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);

	this->width = width;
	this->height = height;
}

void Window::CreateSurface(VkInstance instance)
{
	this->instance = instance;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void Window::GetSize(int* width, int* height)
{
	glfwGetFramebufferSize(window, width, height);
}

void Window::SetFramebufferSizeCallback(const GLFWframebuffersizefun& callback)
{
	glfwSetFramebufferSizeCallback(window, callback);
}

void Window::SetTitle(const char* title)
{
	glfwSetWindowTitle(window, title);
}

void Window::Destroy()
{
	glfwDestroyWindow(window);
}

void Window::SetAsActiveInput()
{
	InputManager::SetWindow(window);
}

void Window::SetFullScreen(FullscreenType type)
{
	if (fullscreenType == FullscreenType::Windowed && type != FullscreenType::Windowed)
	{
		glfwGetWindowPos(window, &xpos, &ypos);
		glfwGetWindowSize(window, &width, &height);
	}

	GLFWmonitor* monitor = get_current_monitor(window);
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	if (resX <= 0 || resY <= 0)
	{
		resX = mode->width;
		resY = mode->height;
	}

	fullscreenType = type;

	switch (type)
	{
	case FullscreenType::Windowed:
		glfwSetWindowMonitor(window, NULL, xpos, ypos, width, height, 0);
		break;

	case FullscreenType::Fullscreen:
		glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_TRUE);
		glfwSetWindowMonitor(window, monitor, 0, 0, resX, resY, mode->refreshRate);
		break;

	case FullscreenType::Borderless:
		glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
		glfwSetWindowMonitor(window, monitor, 0, 0, resX, resY, mode->refreshRate);
		break;
	}
}

void Window::SetResolution(int width, int height, bool updateImmediately)
{
	resX = width;
	resY = height;
	if (fullscreenType == FullscreenType::Windowed)
	{
		glfwGetWindowPos(window, &xpos, &ypos);
		this->width = width;
		this->height = height;
	}

	if (updateImmediately)
		SetFullScreen(fullscreenType);
}


bool Window::ShouldClose()
{
	return glfwWindowShouldClose(window);
}



static int mini(int x, int y)
{
	return x < y ? x : y;
}

static int maxi(int x, int y)
{
	return x > y ? x : y;
}

//Source: https://stackoverflow.com/a/31526753
GLFWmonitor* Window::get_current_monitor(GLFWwindow* window)
{
	int nmonitors, i;
	int wx, wy, ww, wh;
	int mx, my, mw, mh;
	int overlap, bestoverlap;
	GLFWmonitor* bestmonitor;
	GLFWmonitor** monitors;
	const GLFWvidmode* mode;

	bestoverlap = 0;
	bestmonitor = glfwGetPrimaryMonitor(); // just in case of no overlap

	glfwGetWindowPos(window, &wx, &wy);
	glfwGetWindowSize(window, &ww, &wh);
	monitors = glfwGetMonitors(&nmonitors);

	for (i = 0; i < nmonitors; i++) {
		mode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		mw = mode->width;
		mh = mode->height;

		overlap =
			maxi(0, mini(wx + ww, mx + mw) - maxi(wx, mx)) *
			maxi(0, mini(wy + wh, my + mh) - maxi(wy, my));

		if (bestoverlap < overlap) {
			bestoverlap = overlap;
			bestmonitor = monitors[i];
		}
	}

	return bestmonitor;
}