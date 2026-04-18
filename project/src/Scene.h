#pragma once
#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS          // enable file I/O
#define TINYGLTF3_ENABLE_STB_IMAGE   // enable image decoding
#include <tiny_gltf_v3.h>

class Scene
{
public:
	Scene();
	bool LoadGLTF(std::string filename);
	bool LoadOBJ(std::string filename);

private:
	tinygltf3::Model* model{};

};