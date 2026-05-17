#pragma once
#include "structs.h"
#include "Texture.h"

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

	bool isOpaque{};

	void Initialize();

private:
	void createVertexBuffer();
	void createIndexBuffer();
};

struct Material
{
public:
	Material(Texture* color);
	Material(Texture* color, Texture* normal);
	Material(Texture* color, Texture* normal, Texture* metalic_roughness);
	~Material();

	Texture* color{};
	Texture* normal{};
	Texture* metalic_roughness{};

	std::vector<VkDescriptorSet> descriptorSets{}; // one for every frame in flight

	void Initialize();
};

class Model
{
public:
	Model() = default;
	~Model();

	std::vector<Primitive*> primitives{};
	std::vector<Material*> materials{};

	void Initialize();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer);
};
