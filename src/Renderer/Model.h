#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Buffer.h"
#include "Texture.h"

namespace MilkShake
{
	namespace Graphics
	{
		class VKRenderer;

		struct VertexObject
		{
			glm::vec3 position;
			glm::vec3 normal;
			glm::vec3 color;
			glm::vec2 texCoord;
		};

		class Model
		{
			private:
				struct Primitive
				{
					uint32_t firstIndex;
					uint32_t indexCount;
					int32_t materialIndex;
				};
				struct Mesh
				{
					std::vector<Primitive> primitives;
				};
				struct Node
				{
					Node* parent;
					std::vector<Node*> children;
					Mesh mesh;
					glm::mat4 matrix;

					~Node()
					{
						for (Node* child : children)
						{
							delete child;
						}
					}
				};

			public:
				Model() = default;
				~Model();
				void LoadModel(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, const std::filesystem::path& _filePath);

				const std::vector<VertexObject>& GetVertices() const { return m_Vertices; }
				const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
				const std::vector<Texture*>& GetTextures() const { return m_Textures; }
				const std::vector<Node*>& GetNode() const { return m_Nodes; }
				const std::vector<Node*>& GetLinearNode() const { return m_LinearNodes; }
				const Buffer& GetVertexBuffer() const { return m_VertexBuffer; }
				const Buffer& GetIndexBuffer() const { return m_IndexBuffer; }

			private:
				/*
				VkDevice* m_pDevice;
				VkDescriptorPool m_DescriptorPool;
				*/

				Buffer m_VertexBuffer;
				Buffer m_IndexBuffer;
				std::vector<VertexObject> m_Vertices;
				std::vector<uint32_t> m_Indices;
				std::vector<Texture*> m_Textures;

				std::vector<Node*> m_Nodes;
				std::vector<Node*> m_LinearNodes;

				void ProcessNode(aiNode* _node, const aiScene* _scene, Node* _parent);
				void ProcessMesh(aiMesh* _mesh, const aiScene* _scene, Node* _currentNode);
		};
	}
}