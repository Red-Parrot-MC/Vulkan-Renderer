#pragma once
#include "../core/Window.h"
#include "Scene.h"
#include "Camera.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const std::string WINDOW_NAME = "Vulkan";

class Scene;

class BaseApplication
{
public:
	void run()
	{
        CONTEXT = new VulkanContext();
        window.Create(WIDTH, HEIGHT, WINDOW_NAME.c_str());
        window.SetAsActiveInput();
		initVulkan();
        Initialize();
		mainLoop();
        Cleanup();
        this->~BaseApplication();
		cleanupVulkan();
	}

protected:
    Scene* scene{};
    Camera camera{};
    Window window{};
    virtual ~BaseApplication() {};

private:
    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    virtual void Cleanup() {}

    void initVulkan();
    void mainLoop();
    void cleanupVulkan();
    void drawFrame();
    void RecreateSwapChain();
    void RecordCommandBuffer(uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentImage);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};