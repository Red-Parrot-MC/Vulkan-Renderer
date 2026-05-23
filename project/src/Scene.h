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
	Material* createDefaultTextures();

};