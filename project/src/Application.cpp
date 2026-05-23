#include "Application.h"

void Application::Initialize()
{
	scene->LoadGLTF("resources/sponza/Sponza.gltf");
	scene->LoadGLTF("resources/Cactus/Cactus.gltf");

    std::cout << "\n\n\nCONTROLS: \n\tMoving: [WASDQE] \n\tLooking: [ARROWS / RMB]\n\n";
}

void Application::Update(float deltaTime)
{
	UpdateCamera(deltaTime);
}

void Application::Cleanup()
{

}

void Application::UpdateCamera(float deltaTime)
{
	camera.ApplyDefaultMovement(deltaTime);
}