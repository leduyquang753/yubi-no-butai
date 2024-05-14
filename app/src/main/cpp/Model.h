#ifndef YUBINOBUTAI_MODEL_H
#define YUBINOBUTAI_MODEL_H

#include <vector>

#include "BasicData.h"
#include "TextureAsset.h"

class Model {
	private:
		std::vector<Vertex> vertices;
		std::vector<Index> indices;
		std::shared_ptr<TextureAsset> texture;
	public:
		inline Model(
			std::vector<Vertex> vertices, std::vector<Index> indices, std::shared_ptr<TextureAsset> texture
		): vertices(std::move(vertices)), indices(std::move(indices)), texture(std::move(texture)) {}

		inline const Vertex *getVertexData() const {
			return vertices.data();
		}

		inline const size_t getIndexCount() const {
			return indices.size();
		}

		inline const Index *getIndexData() const {
			return indices.data();
		}

		inline const TextureAsset &getTexture() const {
			return *texture;
		}
};

#endif // YUBINOBUTAI_MODEL_H