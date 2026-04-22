#include "Scene.h"
#include <iostream>
#include <tiny_obj_loader.h>
#include <fstream>
#include "VulkanContext.h"

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS          // enable file I/O
#define TINYGLTF3_ENABLE_STB_IMAGE   // enable image decoding
#include <tiny_gltf_v3.h>

Scene::Scene()
{

}

Scene::~Scene()
{
    for (Model* model : models)
    {
        delete model;
    }
}

bool Scene::LoadOBJ(std::string filename)
{
    Model* model = new Model();

    model->vertices.clear();

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

                model->vertices.push_back(vertex);
            }
            index_offset += fv;

            // per-face material
            //shapes[s].mesh.material_ids[f];
        }
    }

    for (size_t i = 0; i < model->vertices.size(); i++) model->indices.push_back(i);

    models.push_back(model);

	return true;
}

bool Scene::LoadGLTF(std::string filename)
{
	tinygltf3::Model model{};
	tinygltf3::ErrorStack errorStack{};

	tg3_error_code errorCode = tinygltf3::parse_file(model, errorStack, filename.c_str());

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

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

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

    for (size_t i = 0; i < models.size(); i++)
    {
        VkBuffer vertexBuffers[] = { models[i]->vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, models[i]->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->pipelineLayout, 0, 1, &models[i]->descriptorSets[CONTEXT->currentFrame], 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(models[i]->indices.size()), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}
