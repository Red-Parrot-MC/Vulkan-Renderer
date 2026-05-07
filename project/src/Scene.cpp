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
#include <unordered_map>
#pragma warning(default : 4996)

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
    std::unordered_map<uint32_t, uint32_t> loadedMaterials{};

    uint32_t primitivesCount{ 0 };
    uint32_t primitivesLoaded{ 0 };

    for (uint32_t meshIndex = 0; meshIndex < tg3Model->meshes_count; meshIndex++)
    {
        tg3_mesh mesh = tg3Model->meshes[meshIndex];
        primitivesCount += mesh.primitives_count;
    }

    for (uint32_t meshIndex = 0; meshIndex < tg3Model->meshes_count; meshIndex++)
    {
        tg3_mesh mesh = tg3Model->meshes[meshIndex];

        for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; primitiveIndex++)
        {
            primitivesLoaded++;
            std::cout << "\r\033[K" << float(primitivesLoaded) / primitivesCount * 100 << "%";

            tg3_primitive tg3Primitive = mesh.primitives[primitiveIndex];
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

            primitive->vertices.reserve(positions.size());
            glm::vec3 scale = { tg3Model->nodes->scale[0], tg3Model->nodes->scale[1], tg3Model->nodes->scale[2]};

            for (size_t i = 0; i < positions.size(); i++)
            {
                Vertex vertex{};
                vertex.pos = positions[i] * scale;
                if (normals.size() > i)
                    vertex.normal = normals[i];
                if (tangents.size() > i)
                    vertex.tangent = tangents[i];
                if (texcoords.size() > i)
                    vertex.texCoord = texcoords[i];

                primitive->vertices.push_back(vertex);
            }

            uint32_t materialIndex = loadedMaterials[tg3Primitive.material];
            if (materialIndex == 0)
            {
                tg3_material material = tg3Model->materials[tg3Primitive.material];

                Texture* albedo{}, * normal{}, * metallicRoughness{};

                tg3_texture_info baseColorInfo = material.pbr_metallic_roughness.base_color_texture;
                if (baseColorInfo.index > -1)
                {
                    tg3_image baseColorImage = tg3Model->images[tg3Model->textures[baseColorInfo.index].source];
                    albedo = new Texture(std::string(baseUri + baseColorImage.uri.data).c_str());
                }

                tg3_normal_texture_info normalInfo = material.normal_texture;
                if (normalInfo.index > -1)
                {
                    tg3_image normalImage = tg3Model->images[tg3Model->textures[normalInfo.index].source];
                    normal = new Texture(std::string(baseUri + normalImage.uri.data).c_str(), VK_FORMAT_R8G8B8A8_UNORM);
                }

                tg3_texture_info metallicRoughnessInfo = material.pbr_metallic_roughness.metallic_roughness_texture;
                if (metallicRoughnessInfo.index > -1)
                {
                    tg3_image metallicRoughnessImage = tg3Model->images[tg3Model->textures[metallicRoughnessInfo.index].source];
                    metallicRoughness = new Texture(std::string(baseUri + metallicRoughnessImage.uri.data).c_str(), VK_FORMAT_R8G8B8A8_UNORM);
                }

                model->materials.push_back(new Material{ albedo, normal, metallicRoughness });
                materialIndex = model->materials.size();

                loadedMaterials[tg3Primitive.material] = materialIndex;
            }

            primitive->materialIndex = materialIndex - 1;
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
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = CONTEXT->renderPass;
    renderPassInfo.framebuffer = CONTEXT->swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = CONTEXT->swapChainExtent;

    std::vector<VkClearValue> clearValues{
        { {0.0f, 0.0f, 0.0f, 1.0f} },
        { 1.0f, 0 }
    };

    for (gBuffer* b : CONTEXT->gBuffers)
    {
        clearValues.push_back(b->clearValue);
    }

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)CONTEXT->swapChainExtent.width;
    viewport.height = (float)CONTEXT->swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = CONTEXT->swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    for (Model* model : models)
    {
        model->RecordCommandBuffer(commandBuffer);
    }
}
