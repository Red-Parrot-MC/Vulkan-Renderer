#include "Application.h"
#include "DirectionalShadowLight.h"

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
	//scene->LoadGLTF("resources/sponza/Sponza.gltf");
	//scene->LoadGLTF("resources/Cactus/Cactus.gltf");
    scene->LoadGLTF("resources/ABeautifulGame/ABeautifulGame.gltf");
    scene->CalculateBounds();
    camera.nearPlane = 0.01f;

    float pitch = -45;
    float yaw = -90;
    float size = 5;
    float stepSize = 1.f;

    //float s = size / 2;
    //for (float p = pitch - s; p < pitch + s; p += stepSize)
    //{
    //    for (float y = yaw - s; y < yaw + s; y += stepSize)
    //    {
    //        AddShadowLight(p, y, 0.4f, { 0.5, 0.5, 1 });
    //    }
    //}


    AddShadowLight(pitch, yaw, 5, { 1, 1, 1 });

	//for (int i = scene->lights.size(); i < CONTEXT->MAX_LIGHTS; i++)
	//{
 //       scene->lights.push_back(Light({ randomFloat(-8, 8), 0.2f, randomFloat(-5, 5) }, 150, hsv2rgb({randomFloat(0, 360), 1, 1}), LightType::Point));
	//}

	rotationData.resize(scene->lights.size(), glm::vec3(0));

    std::cout << "\n\n\nCONTROLS: \n\tMoving: [WASDQE] \n\tLooking: [ARROWS / RMB]\n\n";

    scene->cubemap.Load("resources/textures/circus_arena_4k.hdr");
}

void Application::Update(float deltaTime)
{
	UpdateCamera(deltaTime);

    //for (auto light : shadowLights)
    //    light->UpdateDepthMap(*scene);

	// x = rotation
	// y = rotationVelocity
	// z = time until velocity changes
	for (int i = 0; i < rotationData.size(); i++)
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

    for (int i = 0; i < rotationData.size(); i++)
    {
        glm::vec3 mult = hsv2rgb({ brightHue, 1, 1 }) * scene->lights[i].color;
        float brightness = length(mult) * 10;
        scene->lights[i].intensity = brightness;
    }
}

void Application::Cleanup()
{
    for (auto light : shadowLights)
    {
        light->Destroy();
        delete light;
    }
    shadowLights.clear();
}

void Application::UpdateCamera(float deltaTime)
{
	camera.ApplyDefaultMovement(deltaTime, 0.5f);
}

void Application::AddShadowLight(float pitch, float yaw, float intensity, glm::vec3 color)
{
    pitch = pitch / 180 * PI;
    yaw = yaw / 180 * PI;

    glm::vec3 forward{};
    forward.x = cos(pitch) * cos(yaw);
    forward.y = sin(pitch);
    forward.z = cos(pitch) * sin(yaw);
    forward = glm::normalize(forward);

    shadowLights.push_back(new DirectionalShadowLight{ Light(forward, intensity, color, LightType::Directional), *scene });
    scene->directionalShadowLights.push_back(shadowLights.back()->GetBufferObject());
}