#include "Scene.h"
#include <iostream>
#include <tiny_obj_loader.h>
#include <fstream>

bool Scene::LoadOBJ(std::string filename)
{
	

	return false;
}

bool Scene::LoadGLTF(std::string filename)
{
	tinygltf3::ErrorStack errorStack{};

	tg3_error_code errorCode = tinygltf3::parse_file(*model, errorStack, filename.c_str());

	for (uint32_t i = 0; i < errorStack.count(); i++)
	{
		std::cerr << errorStack.entry(i)->message;
	}

	if (errorCode != tg3_error_code::TG3_OK)
	{
		return false;
	}

	return true;
}