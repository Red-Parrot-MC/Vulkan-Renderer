#include "BaseApplication.h"

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

	// Members
	double m_cursorX{};
	double m_cursorY{};

	std::vector<glm::vec3> rotationData{};
	float brightHue{};
};