#include "loader/gltf_nodes.h"
#include "scene/model.h"
#include "scene/node.h"
#include "core/math.h"
#include <glfw/glfw3.h>
#include "tiny_gltf.h"
void processNode(
	const tinygltf::Model& gltf,
	const tinygltf::Node& node,
	Node* parent,
	const std::string& baseDir,
	Model* model)
{
	Node* newNode = new Node();
	newNode->name = node.name;

	// ─────────────────────────────────────────────
	// Node transform
	// ─────────────────────────────────────────────

	if (!node.matrix.empty()) {
		newNode->matrix = glm::make_mat4(node.matrix.data());
		newNode->translation = glm::vec3(0.0f);
		newNode->rotation = glm::quat(1, 0, 0, 0);
		newNode->scale = glm::vec3(1.0f);
	}
	else {
		if (!node.translation.empty())
			newNode->translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
		if (!node.rotation.empty())
			newNode->rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
		if (!node.scale.empty())
			newNode->scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
	}

	if (parent) {
		newNode->parent = parent;
		parent->children.push_back(newNode);
	}

	// ─────────────────────────────────────────────
	// Helper: decode normalized integer → float
	// ─────────────────────────────────────────────
	auto readFloat = [](const unsigned char* src, int componentType) -> float {
		switch (componentType) {
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return *reinterpret_cast<const float*>(src);

		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return (*src) / 255.0f;

		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return (*reinterpret_cast<const uint16_t*>(src)) / 65535.0f;

		default:
			throw std::runtime_error("Unsupported componentType for float conversion");
		}
		};

	// ─────────────────────────────────────────────
	// Process mesh
	// ─────────────────────────────────────────────
	if (node.mesh >= 0) {
		const tinygltf::Mesh& mesh = gltf.meshes[node.mesh];

		for (const auto& primitive : mesh.primitives) {
			Mesh* newMesh = new Mesh;

			// ─────────────────────────────────────────────
			// Index buffer
			// ─────────────────────────────────────────────
			const auto& indexAccessor = gltf.accessors[primitive.indices];
			const auto& indexBufferView = gltf.bufferViews[indexAccessor.bufferView];
			const auto& indexBuffer = gltf.buffers[indexBufferView.buffer];

			size_t indexCount = indexAccessor.count;

			const unsigned char* indexData =
				&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

			size_t indexStride = 0;
			switch (indexAccessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indexStride = 2; break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   indexStride = 4; break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  indexStride = 1; break;
			default: throw std::runtime_error("Unsupported index type");
			}

			// ─────────────────────────────────────────────
			// POSITION (float only)
			// ─────────────────────────────────────────────
			const auto& posAccessor = gltf.accessors[primitive.attributes.at("POSITION")];
			const auto& posBufferView = gltf.bufferViews[posAccessor.bufferView];
			const auto& posBuffer = gltf.buffers[posBufferView.buffer];

			if (posAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
				throw std::runtime_error("POSITION must be FLOAT");

			size_t posStride = posBufferView.byteStride ?
				posBufferView.byteStride :
				3 * sizeof(float);

			// ─────────────────────────────────────────────
			// NORMAL (float only)
			// ─────────────────────────────────────────────
			bool hasNormals = primitive.attributes.count("NORMAL");
			const tinygltf::Accessor* normalAccessor = nullptr;
			const tinygltf::BufferView* normalBufferView = nullptr;
			const tinygltf::Buffer* normalBuffer = nullptr;
			size_t normalStride = 0;

			if (hasNormals) {
				normalAccessor = &gltf.accessors[primitive.attributes.at("NORMAL")];
				if (normalAccessor->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					throw std::runtime_error("NORMAL must be FLOAT");

				normalBufferView = &gltf.bufferViews[normalAccessor->bufferView];
				normalBuffer = &gltf.buffers[normalBufferView->buffer];
				normalStride = normalBufferView->byteStride ?
					normalBufferView->byteStride :
					3 * sizeof(float);
			}
			// ─────────────────────────────────────────────
			// TEXCOORD_0 (float or normalized int)
			// ─────────────────────────────────────────────
			bool hasTexCoords0 = primitive.attributes.count("TEXCOORD_0") > 0;
			const tinygltf::Accessor* uvAccessor = nullptr;
			const tinygltf::BufferView* uvBufferView = nullptr;
			const tinygltf::Buffer* uvBuffer = nullptr;
			size_t uvStride = 0;

			if (hasTexCoords0) {
				uvAccessor = &gltf.accessors[primitive.attributes.at("TEXCOORD_0")];
				uvBufferView = &gltf.bufferViews[uvAccessor->bufferView];
				uvBuffer = &gltf.buffers[uvBufferView->buffer];

				if (uvAccessor->type != TINYGLTF_TYPE_VEC2)
					throw std::runtime_error("TEXCOORD_0 must be VEC2");

				if (uvBufferView->byteStride != 0) {
					uvStride = uvBufferView->byteStride;
				}
				else {
					switch (uvAccessor->componentType) {
					case TINYGLTF_COMPONENT_TYPE_FLOAT:          uvStride = 2 * sizeof(float);   break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  uvStride = 2 * sizeof(uint8_t); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: uvStride = 2 * sizeof(uint16_t); break;
					default:
						throw std::runtime_error("Unsupported TEXCOORD_0 componentType");
					}
				}
			}

			// ─────────────────────────────────────────────
			// TEXCOORD_1 (float or normalized int)
			// ─────────────────────────────────────────────
			bool hasTexCoords1 = primitive.attributes.count("TEXCOORD_1") > 0;
			const tinygltf::Accessor* uv1Accessor = nullptr;
			const tinygltf::BufferView* uv1BufferView = nullptr;
			const tinygltf::Buffer* uv1Buffer = nullptr;
			size_t uv1Stride = 0;

			if (hasTexCoords1) {
				uv1Accessor = &gltf.accessors[primitive.attributes.at("TEXCOORD_1")];
				uv1BufferView = &gltf.bufferViews[uv1Accessor->bufferView];
				uv1Buffer = &gltf.buffers[uv1BufferView->buffer];

				if (uv1Accessor->type != TINYGLTF_TYPE_VEC2)
					throw std::runtime_error("TEXCOORD_1 must be VEC2");

				if (uv1BufferView->byteStride != 0) {
					uv1Stride = uv1BufferView->byteStride;
				}
				else {
					switch (uv1Accessor->componentType) {
					case TINYGLTF_COMPONENT_TYPE_FLOAT:          uv1Stride = 2 * sizeof(float);   break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  uv1Stride = 2 * sizeof(uint8_t); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: uv1Stride = 2 * sizeof(uint16_t); break;
					default:
						throw std::runtime_error("Unsupported TEXCOORD_1 componentType");
					}
				}
			}

			// ─────────────────────────────────────────────
			// COLOR_0 (float or normalized int)
			// ─────────────────────────────────────────────
			bool hasColors = primitive.attributes.count("COLOR_0");
			const tinygltf::Accessor* colorAccessor = nullptr;
			const tinygltf::BufferView* colorBufferView = nullptr;
			const tinygltf::Buffer* colorBuffer = nullptr;
			size_t colorStride = 0;

			if (hasColors) {
				colorAccessor = &gltf.accessors[primitive.attributes.at("COLOR_0")];
				colorBufferView = &gltf.bufferViews[colorAccessor->bufferView];
				colorBuffer = &gltf.buffers[colorBufferView->buffer];

				int comps = (colorAccessor->type == TINYGLTF_TYPE_VEC3 ? 3 :
					colorAccessor->type == TINYGLTF_TYPE_VEC4 ? 4 : 0);
				if (!comps) throw std::runtime_error("COLOR_0 must be VEC3 or VEC4");

				if (colorBufferView->byteStride)
					colorStride = colorBufferView->byteStride;
				else {
					switch (colorAccessor->componentType) {
					case TINYGLTF_COMPONENT_TYPE_FLOAT:          colorStride = comps * 4; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  colorStride = comps * 1; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: colorStride = comps * 2; break;
					default: throw std::runtime_error("Unsupported COLOR_0 componentType");
					}
				}
			}

			// ─────────────────────────────────────────────
			// TANGENT (float only)
			// ─────────────────────────────────────────────
			bool hasTangents = primitive.attributes.count("TANGENT");
			const tinygltf::Accessor* tanAccessor = nullptr;
			const tinygltf::BufferView* tanBufferView = nullptr;
			const tinygltf::Buffer* tanBuffer = nullptr;
			size_t tanStride = 0;

			if (hasTangents) {
				tanAccessor = &gltf.accessors[primitive.attributes.at("TANGENT")];
				if (tanAccessor->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					throw std::runtime_error("TANGENT must be FLOAT");

				tanBufferView = &gltf.bufferViews[tanAccessor->bufferView];
				tanBuffer = &gltf.buffers[tanBufferView->buffer];
				tanStride = tanBufferView->byteStride ?
					tanBufferView->byteStride :
					4 * sizeof(float);
			}

			// ─────────────────────────────────────────────
			// Vertex loop
			// ─────────────────────────────────────────────
			uint32_t baseVertex = (uint32_t)newMesh->vertices.size();
			newMesh->vertices.reserve(newMesh->vertices.size() + posAccessor.count);

			for (size_t i = 0; i < posAccessor.count; ++i) {
				Vertex v{};

				// POSITION
				const float* pos = reinterpret_cast<const float*>(
					&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * posStride]);
				v.pos = { pos[0], pos[1], pos[2] };

				// NORMAL
				if (hasNormals) {
					const float* n = reinterpret_cast<const float*>(
						&normalBuffer->data[normalBufferView->byteOffset + normalAccessor->byteOffset + i * normalStride]);
					v.normal = { n[0], n[1], n[2] };
				}

				// TEXCOORD_0
				if (hasTexCoords0) {
					const unsigned char* base = &uvBuffer->data[
						uvBufferView->byteOffset +
							uvAccessor->byteOffset +
							i * uvStride
					];

					switch (uvAccessor->componentType) {
					case TINYGLTF_COMPONENT_TYPE_FLOAT: {
						const float* uv = reinterpret_cast<const float*>(base);
						v.texCoord0 = { uv[0], uv[1] };
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
						const uint8_t* uv = reinterpret_cast<const uint8_t*>(base);
						v.texCoord0 = {
							uv[0] / 255.0f,
							uv[1] / 255.0f
						};
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
						const uint16_t* uv = reinterpret_cast<const uint16_t*>(base);
						v.texCoord0 = {
							uv[0] / 65535.0f,
							uv[1] / 65535.0f
						};
						break;
					}
					default:
						v.texCoord0 = { 0.0f, 0.0f }; // should never hit due to earlier check
						break;
					}
				}
				else {
					v.texCoord0 = { 0.0f, 0.0f };
				}

				// TEXCOORD_1
				if (hasTexCoords1) {
					const unsigned char* base = &uv1Buffer->data[
						uv1BufferView->byteOffset +
							uv1Accessor->byteOffset +
							i * uv1Stride
					];

					switch (uv1Accessor->componentType) {
					case TINYGLTF_COMPONENT_TYPE_FLOAT: {
						const float* uv = reinterpret_cast<const float*>(base);
						v.texCoord1 = { uv[0], uv[1] };
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
						const uint8_t* uv = reinterpret_cast<const uint8_t*>(base);
						v.texCoord1 = {
							uv[0] / 255.0f,
							uv[1] / 255.0f
						};
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
						const uint16_t* uv = reinterpret_cast<const uint16_t*>(base);
						v.texCoord1 = {
							uv[0] / 65535.0f,
							uv[1] / 65535.0f
						};
						break;
					}
					default:
						v.texCoord1 = { 0.0f, 0.0f }; // should never hit
						break;
					}
				}
				else {
					v.texCoord1 = v.texCoord0; // fallback
				}

				// COLOR_0
				if (hasColors) {
					const unsigned char* base = &colorBuffer->data[
						colorBufferView->byteOffset + colorAccessor->byteOffset + i * colorStride];

					v.color.r = readFloat(base + 0 * (colorStride / (colorAccessor->type == TINYGLTF_TYPE_VEC4 ? 4 : 3)), colorAccessor->componentType);
					v.color.g = readFloat(base + 1 * (colorStride / (colorAccessor->type == TINYGLTF_TYPE_VEC4 ? 4 : 3)), colorAccessor->componentType);
					v.color.b = readFloat(base + 2 * (colorStride / (colorAccessor->type == TINYGLTF_TYPE_VEC4 ? 4 : 3)), colorAccessor->componentType);
				}
				else {
					v.color = { 1,1,1 };
				}

				// TANGENT
				v.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
				if (hasTangents) {
					const float* t = reinterpret_cast<const float*>(
						&tanBuffer->data[tanBufferView->byteOffset + tanAccessor->byteOffset + i * tanStride]);
					v.tangent = { t[0], t[1], t[2], t[3] };
				}

				newMesh->vertices.push_back(v);
			}
			// ─────────────────────────────────────────────
			// Compute mesh bounds + center (minimal fix)
			// ─────────────────────────────────────────────
			newMesh->minBounds = glm::vec3(FLT_MAX);
			newMesh->maxBounds = glm::vec3(-FLT_MAX);

			for (const Vertex& v : newMesh->vertices)
			{
				newMesh->minBounds = glm::min(newMesh->minBounds, v.pos);
				newMesh->maxBounds = glm::max(newMesh->maxBounds, v.pos);
			}

			newMesh->center = 0.5f * (newMesh->minBounds + newMesh->maxBounds);

			// ─────────────────────────────────────────────
			// Index loop
			// ─────────────────────────────────────────────
			newMesh->indices.reserve(newMesh->indices.size() + indexCount);

			for (size_t i = 0; i < indexCount; ++i) {
				uint32_t idx = 0;
				const unsigned char* src = indexData + i * indexStride;

				switch (indexAccessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: idx = *reinterpret_cast<const uint16_t*>(src); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   idx = *reinterpret_cast<const uint32_t*>(src); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  idx = *reinterpret_cast<const uint8_t*>(src);  break;
				}

				newMesh->indices.push_back(baseVertex + idx);
			}

			if (primitive.material >= 0)
				newMesh->materialIndex = model->baseMaterialIndex + primitive.material;


			newNode->meshes.push_back(newMesh);
		}
	}

	// ─────────────────────────────────────────────
	// Recurse
	// ─────────────────────────────────────────────
	for (int child : node.children)
		processNode(gltf, gltf.nodes[child], newNode, baseDir, model);
}

void parseSceneNodes(
	const tinygltf::Model& gltf,
	Model* model,
	const std::string& baseDir)
{
	// glTF may have zero scenes
	if (gltf.scenes.empty())
	{
		// Create an empty root node and bail out
		return;
	}

	int sceneIndex = gltf.defaultScene;

	// If defaultScene is invalid, fall back to 0
	if (sceneIndex < 0 || sceneIndex >= (int)gltf.scenes.size())
		sceneIndex = 0;

	const tinygltf::Scene& scene = gltf.scenes[sceneIndex];

	for (int nodeIndex : scene.nodes)
	{
		const tinygltf::Node& node = gltf.nodes[nodeIndex];
		processNode(gltf, node, model->rootNode, baseDir, model);
	}
}
