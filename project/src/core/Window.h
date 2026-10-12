#pragma once
#include "glfwInclude.h"

enum class FullscreenType
{
	Windowed,
	Fullscreen,
	Borderless,
};

class Window
{
public:
	void Create(int width, int height, const char* title);
	void CreateSurface(VkInstance instance);
	void GetSize(int* width, int* height);
	void SetFramebufferSizeCallback(const GLFWframebuffersizefun& callback);
	void SetTitle(const char* title);
	void Destroy();
	void SetAsActiveInput();
	void SetFullScreen(FullscreenType type);
	void SetResolution(int width, int height, bool updateImmediately = true);

	bool ShouldClose();
	VkSurfaceKHR GetSurface() { return surface; }
	FullscreenType GetFullscreenType() { return fullscreenType; }

private:
	GLFWwindow* window{};
	VkSurfaceKHR surface{};
	VkInstance instance{};
	int width{}, height{};
	int xpos{}, ypos{};
	int resX{}, resY{};
	FullscreenType fullscreenType{};

	static GLFWmonitor* get_current_monitor(GLFWwindow* window);
};