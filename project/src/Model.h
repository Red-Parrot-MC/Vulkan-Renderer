#pragma once
#include "structs.h"
#include "Texture.h"

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

enum class LightType
{
	Point,
	Directional,
	Spot // unused
};

struct Light
{
	glm::vec3 pos{};
	glm::vec3 color{};
	float intensity{};
	LightType lightType{};
};

class Model
{
public:
	Model() = default;
	~Model();

	std::vector<Primitive*> primitives{};
	std::vector<Material*> materials{};
	std::vector<Texture*> textures{};
	std::vector<Light> lights{};

	void Initialize();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer);
};
