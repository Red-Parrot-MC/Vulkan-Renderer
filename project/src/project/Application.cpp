#include "Application.h"
#include "../engine/DirectionalShadowLight.h"

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
    if (scene->LoadGLTF("resources/ABeautifulGame/ABeautifulGame.gltf"))
    {
        //scene->models[0]->outline = true;
        //scene->models[0]->outlineColor = glm::vec4(0, 0, 0, 1);
    }

    scene->modelMatrix = glm::translate(scene->modelMatrix, { 0, -0.4f, 0 });
    scene->modelMatrix = glm::scale(scene->modelMatrix, { 20, 20, 20 });

    //scene->LoadGLTF("resources/grass/grass.gltf");
    //scene->models[1]->cullMode = VK_CULL_MODE_NONE;
    //scene->models[1]->outline = true;

    scene->CalculateBounds();

    AddShadowLight(-45, -90, 20, { 155 / 255.f, 201 / 255.f, 255 / 255.f });

    scene->cubemap.Load("resources/textures/circus_arena_4k.hdr"); // circus_arena_4k // mossy_forest_4k

    std::cout << "\n\n\nCONTROLS: \n\tMoving: [WASDQE] \n\tLooking: [ARROWS / RMB]\n\n";
}

void Application::Update(float deltaTime)
{
	UpdateCamera(deltaTime);
    //scene->models[0]->modelMatrix = glm::translate(scene->models[0]->modelMatrix, glm::vec3(0.001f, 0, 0));
    scene->CalculateBounds();
    shadowLights[0]->UpdateDepthMap(*scene);

    if (InputManager::GetKeyDown(GLFW_KEY_F11))
    {
        switch (window.GetFullscreenType())
        {
        case FullscreenType::Windowed:
            window.SetResolution(640, 480, false);
            window.SetFullScreen(FullscreenType::Borderless);
            break;
        case FullscreenType::Fullscreen:
            window.SetFullScreen(FullscreenType::Windowed);
            break;
        case FullscreenType::Borderless:
            window.SetFullScreen(FullscreenType::Windowed);
            break;
        }

        int width, height;
        window.GetSize(&width, &height);
        std::cout << "Size: " << width << ", " << height << "\n";
    }

    if (InputManager::GetMouseButton(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        scene->models[0]->outline = !scene->models[0]->outline;
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