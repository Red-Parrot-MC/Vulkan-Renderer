#include "Application.h"

void Application::Initialize()
{
	scene->LoadGLTF("resources/sponza/Sponza.gltf");
	scene->LoadGLTF("resources/Cactus/Cactus.gltf");
	
	scene->lights.push_back(Light({ 0, 0, 0 }, 0.025f, { 1, 1, 1 }, LightType::Ambient));
	scene->lights.push_back(Light({ 0.577, 0.577, -0.577 }, 0, { 1, 0.83, 0.24 }, LightType::Directional));
	scene->lights.push_back(Light({ 0, 0.2, 0 }, 1, { 1, 1, 1 }, LightType::Point));
	scene->lights.push_back(Light({ 5, 0.2, 0 }, 10, { 0, 1, 0 }, LightType::Point));
	scene->lights.push_back(Light({ -5, 0.2, 0 }, 20, { 0, 0, 1 }, LightType::Point));

    std::cout << "\n\n\nCONTROLS: \n\tMoving: [WASDQE] \n\tLooking: [ARROWS / RMB]\n\n";
}

void Application::Update(float deltaTime)
{
	UpdateCamera(deltaTime);
	scene->lights[2].pos.x += deltaTime;
}

void Application::Cleanup()
{

}

void Application::UpdateCamera(float deltaTime)
{
	camera.ApplyDefaultMovement(deltaTime);
}