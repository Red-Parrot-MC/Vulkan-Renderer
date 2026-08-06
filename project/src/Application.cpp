#include "Application.h"
#include "DirectionalShadowLight.h"

//#define PARTY
//#define SPONZA
//#define ROTATE_LIGHT

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
        out.g = static_cast<float>(t);
        out.b = static_cast<float>(p);
        break;
    case 1:
        out.r = static_cast<float>(q);
        out.g = in.b;
        out.b = static_cast<float>(p);
        break;
    case 2:
        out.r = static_cast<float>(p);
        out.g = in.b;
        out.b = static_cast<float>(t);
        break;

    case 3:
        out.r = static_cast<float>(p);
        out.g = static_cast<float>(q);
        out.b = in.b;
        break;
    case 4:
        out.r = static_cast<float>(t);
        out.g = static_cast<float>(p);
        out.b = in.b;
        break;
    case 5:
    default:
        out.r = in.b;
        out.g = static_cast<float>(p);
        out.b = static_cast<float>(q);
        break;
    }
    return out;
}

void Application::Initialize()
{
#ifdef SPONZA
    scene->LoadGLTF("resources/sponza/Sponza.gltf");
    scene->CalculateBounds();

    AddShadowLight(-80, 45, 100, { 1, 1, 1 });
#else
    scene->LoadGLTF("resources/grass/grass.gltf");
    scene->LoadGLTF("resources/ABeautifulGame/ABeautifulGame.gltf");
    scene->modelMatrix = glm::translate(scene->modelMatrix, { 0, -0.4f, 0 });
    scene->modelMatrix = glm::scale(scene->modelMatrix, { 20, 20, 20 });
    scene->CalculateBounds();

    AddShadowLight(-45, -90, 20, { 1, 1, 1 });
#endif // SPONZA

	//scene->LoadGLTF("resources/Cactus/Cactus.gltf");\

    float pitch = -45;
    float yaw = -90;
    float size = 3.f;
    float stepSize = .5f;

    float s = (size - 1) * stepSize / 2;
    for (float p = pitch - s; p <= pitch + s; p += stepSize)
    {
        for (float y = yaw - s; y <= yaw + s; y += stepSize)
        {
            //AddShadowLight(p, y, 20 / (size * size), {0.5, 0.5, 1});
        }
    }

    std::cout << "Shadow lights: " << shadowLights.size() << "\n";

#ifdef PARTY
    for (int i = scene->lights.size(); i < CONTEXT->MAX_LIGHTS; i++)
    {
        scene->lights.push_back(Light({ randomFloat(-8, 8), 0.f, randomFloat(-5, 5) }, 150, hsv2rgb({ randomFloat(0, 360), 1, 1 }), LightType::Point));
    }
#endif // PARTY

	rotationData.resize(scene->lights.size(), glm::vec3(0));

    std::cout << "\n\n\nCONTROLS: \n\tMoving: [WASDQE] \n\tLooking: [ARROWS / RMB]\n\n";

    scene->cubemap.Load("resources/textures/mossy_forest_4k.hdr");
}

void Application::Update(float deltaTime)
{
	UpdateCamera(deltaTime);
    shadowLights[0]->UpdateDepthMap(*scene);

#ifdef ROTATE_LIGHT
    glm::mat4 rotMatrix{ 1 };
    rotMatrix = glm::rotate(rotMatrix, deltaTime, glm::vec3(0, 1, 0));

    for (auto light : shadowLights)
    {
        light->GetBufferObject()->direction = rotMatrix * glm::vec4(light->GetBufferObject()->direction, 0);
        light->UpdateDepthMap(*scene);
    }
#endif // ROTATE_LIGHT


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

    //brightHue += deltaTime * 60;
    //if (brightHue > 360) brightHue -= 360;

    //for (int i = 0; i < rotationData.size(); i++)
    //{
    //    glm::vec3 mult = hsv2rgb({ brightHue, 1, 1 }) * scene->lights[i].color;
    //    float brightness = length(mult) * 10;
    //    scene->lights[i].intensity = brightness;
    //}
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
	camera.ApplyDefaultMovement(deltaTime, 2.f);
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