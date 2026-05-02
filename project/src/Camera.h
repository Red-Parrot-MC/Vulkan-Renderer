#pragma once
#include "VulkanContext.h"

#define PI 3.1415926f

class Camera
{
public:
	glm::vec3 origin{ 0, 1, 0 };
	float fov{ 45.f };
	float nearPlane{ 0.1f };
	float farPlane{ 100.f };

	glm::vec3 forward{};
	glm::vec3 up{};
	glm::vec3 right{};

	float pitch{};
	float yaw{};

	double cursorX{};
	double cursorY{};

	void Update(float deltaTime)
	{
		float moveSpeed{ 5 };
		float rotateSpeed{ 1 };
		float mouseSensitivity{ 0.4f };

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

		calculateDirections();
	}

private:


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
};