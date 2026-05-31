#pragma once
#include <string>
#include <unordered_map>
#include "Cubamap.h"
#include "DirectionalShadowLight.h"

struct tg3_model;
struct tg3_primitive;
struct tg3_node;

class Scene
{
public:
	Scene();
	~Scene();
	bool LoadGLTF(std::string filename);
	bool LoadOBJ(std::string filename);

	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	std::vector<Model*> models{};
	std::vector<Light> lights{};
	std::vector<DirectionalShadowLightBufferObject*> directionalShadowLights{};
	Cubemap cubemap{};

	void CalculateBounds();
	BoundingBox getBounds() const { return boundingBox; }

private:
	void LoadPrimitiveGLTF(const tg3_model* tg3Model, const tg3_primitive& tg3Primitive, const tg3_node& node, const glm::mat4& modelMatrix, Model* model, std::unordered_map<uint32_t, uint32_t>& loadedMaterials, std::string baseUri);
	Material* createDefaultTextures();

	BoundingBox boundingBox{};
};