#pragma once
#include "VulkanContext.h"
#define PI 3.1415926f

#define EV1

class Camera
{
public:
	Camera()
	{
#ifdef SUNNY_16
		float aperture = 5.f;
		float ISO = 100.f;
		float shutterSpeed = 1.f / 200.f;
		exposure = ConvertEV100ToExposure(CalculateEV100FromPhysicalCamera(aperture, shutterSpeed, ISO));
#endif // SUNNY_16

#ifdef INDOOR
		float aperture = 1.4f;
		float ISO = 1600.f;
		float shutterSpeed = 1.f / 60.f;
		exposure = ConvertEV100ToExposure(CalculateEV100FromPhysicalCamera(aperture, shutterSpeed, ISO));
#endif // INDOOR

#ifdef EV1
		float EV100_HardCoded = 1.f;
		exposure = ConvertEV100ToExposure(EV100_HardCoded);
#endif // EV1

#ifdef EVn4 // EV-4
		float EV100_HardCoded = -4.f;
		exposure = ConvertEV100ToExposure(EV100_HardCoded);
#endif // EVn4
	}

	glm::vec3 origin{ 0, 1, 0 };
	float fov{ 60.f };
	float nearPlane{ 0.1f };
	float farPlane{ 100.f };

	glm::vec3 forward{};
	glm::vec3 up{};
	glm::vec3 right{};

	float pitch{};
	float yaw{};

	float getExposure() { return exposure; }

	void Update(float deltaTime)
	{
		calculateDirections();
	}

	void ApplyDefaultMovement(float deltaTime, float moveSpeed = 5, float rotateSpeed = 1, float mouseSensitivity = 0.4f)
	{
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_LEFT_SHIFT)) moveSpeed *= 2;

		if (glfwGetKey(CONTEXT->window, GLFW_KEY_W)) origin += forward * moveSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_S)) origin -= forward * moveSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_A)) origin += right * moveSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_D)) origin -= right * moveSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_E)) origin += up * moveSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_Q)) origin -= up * moveSpeed * deltaTime;

		if (glfwGetKey(CONTEXT->window, GLFW_KEY_LEFT)) yaw -= rotateSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_RIGHT)) yaw += rotateSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_DOWN)) pitch -= rotateSpeed * deltaTime;
		if (glfwGetKey(CONTEXT->window, GLFW_KEY_UP)) pitch += rotateSpeed * deltaTime;

		double newCursorX{}, newCursorY{};
		glfwGetCursorPos(CONTEXT->window, &newCursorX, &newCursorY);

		if (glfwGetMouseButton(CONTEXT->window, GLFW_MOUSE_BUTTON_2))
		{
			yaw += float(newCursorX - cursorX) * mouseSensitivity / 100;
			pitch -= float(newCursorY - cursorY) * mouseSensitivity / 100;
		}

		cursorX = newCursorX;
		cursorY = newCursorY;

		if (pitch < -PI / 2) pitch = -PI / 2;
		if (pitch > PI / 2) pitch = PI / 2;
	}

	void CameraSettings(float aperture, float shutterTime, float ISO)
	{
		exposure = ConvertEV100ToExposure(CalculateEV100FromPhysicalCamera(aperture, shutterTime, ISO));
	}

private:

	double cursorX{};
	double cursorY{};
	float exposure{};

	void calculateDirections()
	{
		forward.x = cos(pitch) * cos(yaw);
		forward.y = sin(pitch);
		forward.z = cos(pitch) * sin(yaw);
		forward = glm::normalize(forward);

		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		right = glm::normalize(glm::cross(worldUp, forward));
		up = glm::cross(forward, right);
	}

	float CalculateEV100FromPhysicalCamera(float aperture, float shutterTime, float ISO)
	{
		return log2f(powf(aperture, 2) / shutterTime * 100 / ISO);
	}

	float ConvertEV100ToExposure(float EV100)
	{
		const float maxLuminance = 1.2f * powf(2.f, EV100);
		return 1.f / std::max(maxLuminance, 0.0001f);
	}
};