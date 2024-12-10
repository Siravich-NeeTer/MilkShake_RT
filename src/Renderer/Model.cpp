#include "Model.h"

#include "Math/AssimpGLMHelpers.h"
#include "Utilities/VKUtilities.h"

#include "VKRenderer.h"

namespace MilkShake
{
	namespace Graphics
	{
		// TODO: Change function body to Utilities or somewhere else so other class can call this
		std::string GetDirectory(const std::filesystem::path& _path)
		{
			size_t pos = _path.string().find_last_of("\\/");
			return (std::string::npos == pos ? "" : _path.string().substr(0, pos + 1));
		}

		Model::Model(int _id, std::string _name)
			: IAsset(_id, _name)
		{
		}
		Model::~Model()
		{
			m_VertexBuffer.Destroy();
			m_IndexBuffer.Destroy();
		}
		void Model::LoadModel(VKRenderer& _vkRenderer, VkCommandPool& _commandPool, const std::filesystem::path& _filePath)
		{
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(_filePath.string(),
				aiProcess_Triangulate |
				aiProcess_GenSmoothNormals |
				aiProcess_CalcTangentSpace |
				aiProcess_FlipUVs |
				aiProcess_JoinIdenticalVertices |
				aiProcess_PreTransformVertices |
				aiProcess_TransformUVCoords
			);

			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
			{
				throw std::runtime_error("ERROR::ASSIMP " + std::string(importer.GetErrorString()));
			}

			// Get Materials of Model
			for (size_t i = 0; i < scene->mNumMaterials; i++)
			{
				aiMaterial* ai_material = scene->mMaterials[i];
				Material material;

				// TODO: Remove support only BaseColor
				//for (int materialType = 0; materialType < aiTextureType_SPECULAR; materialType++)
				for (int materialType = 0; materialType < AI_TEXTURE_TYPE_MAX; materialType++)
				{
					if (ai_material->GetTextureCount(static_cast<aiTextureType>(materialType)) > 0)
					{
						aiString aiTexturePath;
						ai_material->GetTexture(static_cast<aiTextureType>(materialType), 0, &aiTexturePath);

						std::filesystem::path texturePath = GetDirectory(_filePath) + aiTexturePath.C_Str();

						int textureID = _vkRenderer.LoadTexture(texturePath);

						if (materialType == aiTextureType_DIFFUSE || materialType == aiTextureType_BASE_COLOR)
							material.BaseColorTextureID = textureID;
						else if (materialType == aiTextureType_NORMALS)
							material.NormalTextureID = textureID;
						else if (materialType == aiTextureType_LIGHTMAP || materialType == aiTextureType_AMBIENT_OCCLUSION)
							material.OcclusionTextureID = textureID;
					}
				}

				m_MaterialIDs.push_back(_vkRenderer.CreateMaterial(material));
			}

			// TODO: If Model doesn't have any Material
			/*
			if (m_Textures.empty())
				m_Textures.push_back(new Texture(_vkRenderer, _commandPool, "assets/models/viking_room.png"));
			*/

			ProcessNode(scene->mRootNode, scene, nullptr);

			VkMemoryPropertyFlags memoryPropertyFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			Utility::CreateBuffer(_vkRenderer,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | memoryPropertyFlags,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&m_VertexBuffer, m_Vertices.size() * sizeof(VertexObject), m_Vertices.data());
			Utility::CreateBuffer(_vkRenderer,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | memoryPropertyFlags,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&m_IndexBuffer, m_Indices.size() * sizeof(uint32_t), m_Indices.data());
		}

		void Model::ProcessNode(aiNode* _node, const aiScene* _scene, Node* _parent)
		{
			// Create Node & Add to list
			Node* node = new Node();
			node->matrix = AssimpGLMHelpers::ConvertMatrixToGLMFormat(_node->mTransformation);
			node->parent = _parent;

			if (_parent != nullptr)
				_parent->children.push_back(node);
			else
				m_Nodes.push_back(node);
			m_LinearNodes.push_back(node);

			// process each mesh located at the current node
			for (unsigned int i = 0; i < _node->mNumMeshes; i++)
			{
				// the node object only contains indices to index the actual objects in the scene. 
				// the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
				aiMesh* mesh = _scene->mMeshes[_node->mMeshes[i]];
				ProcessMesh(mesh, _scene, node);
			}
			// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
			for (unsigned int i = 0; i < _node->mNumChildren; i++)
			{
				ProcessNode(_node->mChildren[i], _scene, node);
			}
		}
		void Model::ProcessMesh(aiMesh* _mesh, const aiScene* _scene, Node* _currentNode)
		{
			if (_mesh->mNumVertices == 0)
				return;

			Primitive primitive;
			primitive.firstIndex = m_Indices.size();

			uint32_t startIdx = m_Vertices.size();
			for (unsigned int i = 0; i < _mesh->mNumVertices; i++)
			{
				VertexObject vertex;

				vertex.position = AssimpGLMHelpers::GetGLMVec(_mesh->mVertices[i]);
				vertex.normal = AssimpGLMHelpers::GetGLMVec(_mesh->mNormals[i]);

				if (_mesh->HasTextureCoords(0)) // Check for texture coordinates
				{
					glm::vec2 vec;
					vec.x = _mesh->mTextureCoords[0][i].x;
					vec.y = _mesh->mTextureCoords[0][i].y;
					vertex.uv = vec;
				}
				else
				{
					vertex.uv = glm::vec2(0.0f, 0.0f);
				}

				m_Vertices.push_back(vertex);
			}
			primitive.materialIndex = m_MaterialIDs[_mesh->mMaterialIndex];

			uint32_t indexCount = 0;
			for (unsigned int i = 0; i < _mesh->mNumFaces; i++)
			{
				aiFace face = _mesh->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++)
				{
					m_Indices.push_back(startIdx + face.mIndices[j]);
					indexCount++;
				}
			}

			primitive.indexCount = indexCount;

			_currentNode->mesh.primitives.push_back(primitive);
		}
	}
}