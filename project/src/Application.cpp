#include "Application.h"

float randomFloat(float min, float max)
{
	return rand() / float(RAND_MAX) * (max - min) + min;
}

glm::vec3 hsv2rgb(glm::vec3 in)
{
    double hh, p, q, t, ff;
    long i;
    glm::vec3 out;

    hh = in.r;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.b * (1.0 - in.g);
    q = in.b * (1.0 - (in.g * ff));
    t = in.b * (1.0 - (in.g * (1.0 - ff)));

    switch (i) {
    case 0:
        out.r = in.b;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.b;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.b;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.b;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.b;
        break;
    case 5:
    default:
        out.r = in.b;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}

void Application::Initialize()
{
	scene->LoadGLTF("resources/sponza/Sponza.gltf");
	scene->LoadGLTF("resources/Cactus/Cactus.gltf");
    scene->CalculateBounds();
    std::cout << "Bounding box: (" << scene->boundingBox.min.x << ", " << scene->boundingBox.min.y << ", " << scene->boundingBox.min.z << "), (" << scene->boundingBox.max.x << ", " << scene->boundingBox.max.y << ", " << scene->boundingBox.max.z << ")\n";
	
	scene->lights.push_back(Light({ 0, 0, 0 }, 0.025f, { 1, 1, 1 }, LightType::Ambient));
	scene->lights.push_back(Light({ 0.577, 0.577, -0.577 }, 1, { 1, 0.83, 0.24 }, LightType::Directional));
	//scene->lights.push_back(Light({ 0, 0.2, 0 }, 1, { 1, 1, 1 }, LightType::Point));
	//scene->lights.push_back(Light({ 5, 0.2, 0 }, 10, { 0, 1, 0 }, LightType::Point));
	//scene->lights.push_back(Light({ -5, 0.2, 0 }, 20, { 0, 0, 1 }, LightType::Point));

	for (int i = scene->lights.size(); i < CONTEXT->MAX_LIGHTS; i++)
	{
        scene->lights.push_back(Light({ randomFloat(-8, 8), 0.2f, randomFloat(-5, 5) }, 15, hsv2rgb({randomFloat(0, 360), 1, 1}), LightType::Point));
	}

	rotationData.resize(scene->lights.size(), glm::vec3(0));

    std::cout << "\n\n\nCONTROLS: \n\tMoving: [WASDQE] \n\tLooking: [ARROWS / RMB]\n\n";
}

void Application::Update(float deltaTime)
{
	UpdateCamera(deltaTime);

	// x = rotation
	// y = rotationVelocity
	// z = time until velocity changes
	for (int i = 2; i < rotationData.size(); i++)
	{
		scene->lights[i].pos += glm::vec3{ sin(rotationData[i].x), 0, cos(rotationData[i].x) } * deltaTime;
        scene->lights[i].pos *= glm::vec3{ 1 - deltaTime / 10, 0, 1 - deltaTime / 10 };
		rotationData[i].x += rotationData[i].y * deltaTime;
		rotationData[i].z -= deltaTime;
		if (rotationData[i].z <= 0)
		{
			rotationData[i].z = randomFloat(0.5, 2.5);
			rotationData[i].y = randomFloat(-1, 1);
		}
	}

    brightHue += deltaTime * 60;
    if (brightHue > 360) brightHue -= 360;

    for (int i = 2; i < rotationData.size(); i++)
    {
        glm::vec3 mult = hsv2rgb({ brightHue, 1, 1 }) * scene->lights[i].color;
        float brightness = length(mult) * 10;
        scene->lights[i].intensity = brightness;
    }
}

void Application::Cleanup()
{

}

void Application::UpdateCamera(float deltaTime)
{
	camera.ApplyDefaultMovement(deltaTime);
}