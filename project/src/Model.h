#pragma once
#include "structs.h"
#include "Texture.h"

class Model
{
public:
	Model() = default;
	~Model();

	std::vector<Vertex> vertices{};
	std::vector<uint16_t> indices{};

	std::vector<Texture*> textures{};

	VkBuffer vertexBuffer{};
	VkBuffer indexBuffer{};
	VkDeviceMemory vertexBufferMemory{};
	VkDeviceMemory indexBufferMemory{};

	std::vector<VkDescriptorSet> descriptorSets{};

	void Initialize();

private:
	void createVertexBuffer();
	void createIndexBuffer();
};