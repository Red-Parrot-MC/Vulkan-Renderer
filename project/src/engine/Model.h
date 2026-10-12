#pragma once
#include "../core/structs.h"
#include "../core/Texture.h"

struct BoundingBox
{
	glm::vec3 min{};
	glm::vec3 max{};
};

class Primitive
{
public:
	Primitive() = default;
	~Primitive();

	std::vector<Vertex> vertices{};
	std::vector<uint32_t> indices{};

	uint32_t materialIndex{};

	VkBuffer vertexBuffer{};
	VkBuffer indexBuffer{};
	VkDeviceMemory vertexBufferMemory{};
	VkDeviceMemory indexBufferMemory{};

	BoundingBox boundingBox{};

	bool isOpaque{};

	void Initialize();
	void CalculateBounds();

private:
	void createVertexBuffer();
	void createIndexBuffer();
};

struct Material
{
public:
	Material(Texture* color, Texture* normal, Texture* metalic_roughness);
	~Material();

	Texture* color{};
	Texture* normal{};
	Texture* metalic_roughness{};

	std::vector<VkDescriptorSet> descriptorSets{}; // one for every frame in flight

	void Initialize();
};

enum class LightType : uint32_t
{
	// shader stops when it finds 0
	Point = 1,
	Directional = 2,
	Ambient = 3,
	Spot = 4, // unused
};

struct Light
{
	glm::vec3 pos{};
	float intensity{}; // lumen for point, lux for directional
	glm::vec3 color{};
	LightType lightType{};

	Light() = default;
	Light(glm::vec3 pos, float intensity, glm::vec3 color, LightType lightType);

	void SetColorFromKelvin(float kelvin);
};

class Model
{
public:
	Model() = default;
	~Model();

	std::vector<Primitive*> primitives{};
	std::vector<Material*> materials{};
	std::vector<Texture*> textures{};

	BoundingBox boundingBox{};
	glm::mat4 modelMatrix{ 1 };

	VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
	bool outline = false;
	glm::vec4 outlineColor{ 1, 0.5f, 0, 1 };

	// for model matrix
	//std::vector<VkDescriptorSet> descriptorSets{}; // one for every frame in flight

	void Initialize();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer);

	void CalculateBounds();
};
