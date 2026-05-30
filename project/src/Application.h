#include "BaseApplication.h"
#include "Cubamap.h"

class Application : public BaseApplication
{
public:
	Application()
		: BaseApplication{}
	{ }

private:
	void Initialize() override;
	void Update(float deltaTime) override;
	void Cleanup() override;

	// Methods
	void UpdateCamera(float deltaTime);
	void AddShadowLight(float pitch, float yaw, float intensity, glm::vec3 color);

	// Members
	double m_cursorX{};
	double m_cursorY{};

	std::vector<glm::vec3> rotationData{};
	float brightHue{};

	//DirectionalShadowLight* shadowLight{};
	std::vector<DirectionalShadowLight*> shadowLights{};
};