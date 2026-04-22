#pragma once
#include "Model.h"
#include <string>


class Scene
{
public:
	Scene();
	~Scene();
	bool LoadGLTF(std::string filename);
	bool LoadOBJ(std::string filename);

	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	std::vector<Model*> models{};
private:

	//std::vector<Vertex> vertices{};
	//std::vector<uint16_t> indices{};

	//std::vector<Texture*> textures{};
};