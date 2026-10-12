#include "../engine/BaseApplication.h"
#include "../core/Cubamap.h"

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

	std::vector<DirectionalShadowLight*> shadowLights{};
};