#include "Scene.h"
#include <iostream>
#include <tiny_obj_loader.h>
#include <fstream>
#include "VulkanContext.h"

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS          // enable file I/O
#define TINYGLTF3_ENABLE_STB_IMAGE   // enable image decoding
#pragma warning(disable : 4996) // release mode doesn't build without this
#include <tiny_gltf_v3.h>
#pragma warning(default : 4996)

#include "ImageHelpers.h"

Scene::Scene()
{
    
}

Scene::~Scene()
{
    for (Model* model : models)
    {
        delete model;
    }
    models.clear();
}

bool Scene::LoadOBJ(std::string filename)
{
    Primitive* primitive = new Primitive();

    primitive->vertices.clear();

    std::string inputfile = filename;
    tinyobj::ObjReaderConfig reader_config;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputfile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader [ERROR]: " << reader.Error();
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader [WARNING]: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                Vertex vertex{};

                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                vertex.pos = { vx, vy, vz };

                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0) {
                    tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                    //vertex.normal = { nx, ny, nz };
                }

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

                    vertex.texCoord = { tx, -ty };
                }

                primitive->vertices.push_back(vertex);
            }
            index_offset += fv;
        }
    }

    for (size_t i = 0; i < primitive->vertices.size(); i++) primitive->indices.push_back((uint32_t)i);

    Model* model = new Model();
    model->primitives.push_back(primitive);

    models.push_back(model);
	return true;
}

bool Scene::LoadGLTF(std::string filename)
{
    clock_t startTime = clock();
    std::cout << "\nLoading Scene: " << filename << "\n";

	tinygltf3::Model gltfModel{};
	tinygltf3::ErrorStack errorStack{};

	tg3_error_code errorCode = tinygltf3::parse_file(gltfModel, errorStack, filename.c_str());

	for (uint32_t i = 0; i < errorStack.count(); i++)
	{
		std::cerr << "GLTF: " << errorStack.entry(i)->message;
	}

	if (errorCode != tg3_error_code::TG3_OK)
	{
		return false;
	}

    tg3_model* tg3Model = gltfModel.get();

    for (size_t i = 0; i < filename.size(); i++) 
        if (filename[i] == '\\') 
            filename[i] = '/';

    std::string baseUri = filename.substr(0, filename.find_last_of('/')) + '/';

    Model* model = new Model{};
    model->materials.push_back(createDefaultTextures());
    model->textures.push_back(model->materials[0]->color);
    model->textures.push_back(model->materials[0]->normal);
    model->textures.push_back(model->materials[0]->metalic_roughness);
    std::unordered_map<uint32_t, uint32_t> loadedMaterials{};

    uint32_t primitivesCount{ 0 };
    uint32_t primitivesLoaded{ 0 };

    // calculate model size
    for (uint32_t meshIndex = 0; meshIndex < tg3Model->meshes_count; meshIndex++)
    {
        tg3_mesh mesh = tg3Model->meshes[meshIndex];
        primitivesCount += mesh.primitives_count;
    }

    for (uint32_t nodeIndex = 0; nodeIndex < tg3Model->nodes_count; nodeIndex++)
    {
        const tg3_node& node = tg3Model->nodes[nodeIndex];
        int32_t meshIndex = node.mesh;
        int32_t lightIndex = node.light;

        if (meshIndex > -1)
        {
            const tg3_mesh& mesh = tg3Model->meshes[meshIndex];

            for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; primitiveIndex++)
            {
                primitivesLoaded++;
                std::cout << "\r\033[K" << float(primitivesLoaded) / primitivesCount * 100 << "%";

                LoadPrimitiveGLTF(tg3Model, mesh.primitives[primitiveIndex], node, model, loadedMaterials, baseUri);
            }
        }

        if (lightIndex > -1)
        {
            const tg3_light& tg3Light = tg3Model->lights[lightIndex];
            
            glm::vec3 scale = { node.scale[0], node.scale[1], node.scale[2] };
            glm::vec3 translation = { node.translation[0], node.translation[1], node.translation[2] };
            glm::vec3 rotation = { node.rotation[0], node.rotation[1], node.rotation[2] };

            glm::mat4 modelMatrix = glm::mat4(1.0f);

            // Translation
            modelMatrix = glm::translate(modelMatrix, translation);

            // Rotation
            modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1, 0, 0));
            modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0, 1, 0));
            modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0, 0, 1));

            // Scale
            modelMatrix = glm::scale(modelMatrix, scale);

            Light light{};
            light.pos = glm::vec4(0, 0, 0, 1) * modelMatrix;
            light.color = { tg3Light.color[0], tg3Light.color[1], tg3Light.color[2] };
            light.intensity = tg3Light.intensity;
            std::string type = tg3Light.type.data;
            if (type == "point")
            {
                light.lightType = LightType::Point;
            }
            if (type == "directional")
            {
                light.lightType = LightType::Directional;
            }
            if (type == "spot")
            {
                light.lightType = LightType::Spot;
            }

            lights.push_back(light);
        }
    }

    model->Initialize();
    models.push_back(model);

    clock_t endTime = clock();
    std::cout << "\nLoading time: " << (endTime - startTime) / float(CLOCKS_PER_SEC) << "s\n";

	return true;
}

void Scene::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    for (Model* model : models)
    {
        model->RecordCommandBuffer(commandBuffer);
    }
}

void Scene::LoadPrimitiveGLTF(const tg3_model* tg3Model, const tg3_primitive& tg3Primitive, const tg3_node& node, Model* model, std::unordered_map<uint32_t, uint32_t>& loadedMaterials, std::string baseUri)
{
    tg3_accessor accessor = tg3Model->accessors[tg3Primitive.indices];
    tg3_buffer_view view = tg3Model->buffer_views[accessor.buffer_view];
    tg3_buffer buffer = tg3Model->buffers[view.buffer];
    const uint8_t* data = buffer.data.data + view.byte_offset + accessor.byte_offset;
    size_t count = buffer.data.count;

    Primitive* primitive = new Primitive();
    model->primitives.push_back(primitive);
    const uint8_t* uint8Data{};
    const uint16_t* uint16Data{};
    const uint32_t* uint32Data{};

    switch (accessor.component_type)
    {
    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
        uint8Data = reinterpret_cast<const uint8_t*>(data);
        primitive->indices = { uint8Data, uint8Data + accessor.count };
        break;
    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
        uint16Data = reinterpret_cast<const uint16_t*>(data);
        primitive->indices = { uint16Data, uint16Data + accessor.count };
        break;
    case TG3_COMPONENT_TYPE_UNSIGNED_INT:
        uint32Data = reinterpret_cast<const uint32_t*>(data);
        primitive->indices = { uint32Data, uint32Data + accessor.count };
        break;
    }

    std::vector<glm::vec3> positions{};
    std::vector<glm::vec3> normals{};
    std::vector<glm::vec4> tangents{};
    std::vector<glm::vec2> texcoords{};

    for (uint32_t attributeIndex = 0; attributeIndex < tg3Primitive.attributes_count; attributeIndex++)
    {
        tg3_str_int_pair attribute = tg3Primitive.attributes[attributeIndex];
        std::string key = attribute.key.data;
        accessor = tg3Model->accessors[attribute.value];
        view = tg3Model->buffer_views[accessor.buffer_view];
        buffer = tg3Model->buffers[view.buffer];
        data = buffer.data.data + view.byte_offset + accessor.byte_offset;

        if (key == "POSITION")
        {
            const glm::vec3* vec3Data = reinterpret_cast<const glm::vec3*>(data);
            positions = { vec3Data, vec3Data + accessor.count };
        }
        if (key == "NORMAL")
        {
            const glm::vec3* vec3Data = reinterpret_cast<const glm::vec3*>(data);
            normals = { vec3Data, vec3Data + accessor.count };
        }
        if (key == "TEXCOORD_0")
        {
            const glm::vec2* vec2Data = reinterpret_cast<const glm::vec2*>(data);
            texcoords = { vec2Data, vec2Data + accessor.count };
        }
        if (key == "TANGENT")
        {
            const glm::vec4* vec4Data = reinterpret_cast<const glm::vec4*>(data);
            tangents = { vec4Data, vec4Data + accessor.count };
        }
    }

    if (normals.size() < positions.size())
    {
        std::cout << "\nCalculating normals\n";
        normals.resize(positions.size(), glm::vec3(0));

        // generate normals
        for (uint32_t i = 0; i < primitive->indices.size(); i += 3)
        {
            glm::vec3 v0 = positions[primitive->indices[i + 0]];
            glm::vec3 v1 = positions[primitive->indices[i + 1]];
            glm::vec3 v2 = positions[primitive->indices[i + 2]];

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;

            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            normals[primitive->indices[i + 0]] += faceNormal;
            normals[primitive->indices[i + 1]] += faceNormal;
            normals[primitive->indices[i + 2]] += faceNormal;
        }

        for (glm::vec3& n : normals)
        {
            n = glm::normalize(n);
        }
    }

    if (tangents.size() < positions.size())
    {
        tangents.resize(positions.size(), glm::vec4(0));

        if (texcoords.size() >= positions.size())
        {
            // generate tangents
            std::cout << "\nGenerating tangents\n";
            for (uint32_t i = 0; i < primitive->indices.size(); i += 3)
            {
                uint32_t i0 = primitive->indices[i + 0];
                uint32_t i1 = primitive->indices[i + 1];
                uint32_t i2 = primitive->indices[i + 2];

                const glm::vec3& p0 = positions[i0];
                const glm::vec3& p1 = positions[i1];
                const glm::vec3& p2 = positions[i2];

                const glm::vec2& uv0 = texcoords[i0];
                const glm::vec2& uv1 = texcoords[i1];
                const glm::vec2& uv2 = texcoords[i2];

                glm::vec3 edge1 = p1 - p0;
                glm::vec3 edge2 = p2 - p0;

                glm::vec2 deltaUV1 = uv1 - uv0;
                glm::vec2 deltaUV2 = uv2 - uv0;

                float f =
                    1.0f /
                    (deltaUV1.x * deltaUV2.y -
                        deltaUV2.x * deltaUV1.y);

                glm::vec3 tangent;

                tangent.x =
                    f * (deltaUV2.y * edge1.x -
                        deltaUV1.y * edge2.x);

                tangent.y =
                    f * (deltaUV2.y * edge1.y -
                        deltaUV1.y * edge2.y);

                tangent.z =
                    f * (deltaUV2.y * edge1.z -
                        deltaUV1.y * edge2.z);

                tangents[i0] += glm::vec4{ tangent, 0 };
                tangents[i1] += glm::vec4{ tangent, 0 };
                tangents[i2] += glm::vec4{ tangent, 0 };
            }

            for (glm::vec4& t : tangents)
            {
                t = glm::vec4{ glm::normalize(glm::vec3(t.x, t.y, t.z)), 1 };
            }
        }
        else
            std::cout << "\nMissing tangents\n";
    }

    if (texcoords.size() < positions.size())
    {
        std::cout << "\nMissing texcoords\n";
        texcoords.resize(positions.size(), glm::vec2(0));
    }

    primitive->vertices.reserve(positions.size());
    glm::vec3 scale = { node.scale[0], node.scale[1], node.scale[2] };
    glm::vec3 translation = { node.translation[0], node.translation[1], node.translation[2] };
    glm::vec3 rotation = { node.rotation[0], node.rotation[1], node.rotation[2] };

    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Translation
    modelMatrix = glm::translate(modelMatrix, translation);

    // Rotation
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1, 0, 0));
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0, 1, 0));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0, 0, 1));

    // Scale
    modelMatrix = glm::scale(modelMatrix, scale);

    for (size_t i = 0; i < positions.size(); i++)
    {
        Vertex vertex{};
        vertex.pos = modelMatrix * glm::vec4(positions[i], 1);
        vertex.normal = modelMatrix * glm::vec4(normals[i], 0);
        vertex.tangent = glm::vec4(glm::vec3(modelMatrix * glm::vec4(glm::vec3(tangents[i]), 0)), tangents[i].w);
        vertex.texCoord = texcoords[i];

        primitive->vertices.push_back(vertex);
    }

    uint32_t materialIndex = loadedMaterials[tg3Primitive.material];
    if (materialIndex == 0)
    {
        tg3_material material = tg3Model->materials[tg3Primitive.material];

        Texture* albedo{}, * normal{}, * metallicRoughness{};

        tg3_texture_info baseColorInfo = material.pbr_metallic_roughness.base_color_texture;
        tg3_normal_texture_info normalInfo = material.normal_texture;
        tg3_texture_info metallicRoughnessInfo = material.pbr_metallic_roughness.metallic_roughness_texture;

        if (baseColorInfo.index > -1)
        {
            tg3_image image = tg3Model->images[tg3Model->textures[baseColorInfo.index].source];
            albedo = new Texture(std::string(baseUri + image.uri.data).c_str(), VK_FORMAT_R8G8B8A8_SRGB);
            model->textures.push_back(albedo);
        }
        else albedo = model->materials[0]->color;

        if (normalInfo.index > -1)
        {
            tg3_image image = tg3Model->images[tg3Model->textures[normalInfo.index].source];
            normal = new Texture(std::string(baseUri + image.uri.data).c_str(), VK_FORMAT_R8G8B8A8_UNORM);
            model->textures.push_back(normal);
        }
        else normal = model->materials[0]->normal;

        if (metallicRoughnessInfo.index > -1)
        {
            tg3_image image = tg3Model->images[tg3Model->textures[metallicRoughnessInfo.index].source];
            metallicRoughness = new Texture(std::string(baseUri + image.uri.data).c_str(), VK_FORMAT_R8G8B8A8_UNORM);
            model->textures.push_back(metallicRoughness);
        }
        else metallicRoughness = model->materials[0]->metalic_roughness;

        model->materials.push_back(new Material{ albedo, normal, metallicRoughness });
        materialIndex = model->materials.size();

        loadedMaterials[tg3Primitive.material] = materialIndex;
    }

    primitive->materialIndex = materialIndex - 1;
    primitive->isOpaque = model->materials[materialIndex - 1]->color->isOpaque();
}

Material* Scene::createDefaultTextures()
{
    Texture* color = new Texture("resources/textures/color_default.png", VK_FORMAT_R8G8B8A8_SRGB);
    Texture* normal = new Texture("resources/textures/normal_default.png", VK_FORMAT_R8G8B8A8_UNORM);
    Texture* metallicRoughness = new Texture("resources/textures/metalicRoughness_default.png", VK_FORMAT_R8G8B8A8_UNORM);

    Material* material = new Material(color, normal, metallicRoughness);
    return material;
}

void Scene::CalculateBounds()
{
    boundingBox = {};
    for (Model* model : models)
    {
        if (model->boundingBox.min == model->boundingBox.max)
            model->CalculateBounds();

        glm::vec3 min = glm::vec4{ model->boundingBox.min, 1 } * model->modelMatrix;
        glm::vec3 max = glm::vec4{ model->boundingBox.max, 1 } * model->modelMatrix;
        boundingBox.min = glm::min(boundingBox.min, min);
        boundingBox.min = glm::min(boundingBox.min, max);
        boundingBox.max = glm::max(boundingBox.max, min);
        boundingBox.max = glm::max(boundingBox.max, max);
    }
}
