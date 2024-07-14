#ifndef TEXT_FONTRENDERER_H
#define TEXT_FONTRENDERER_H

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <Shader.h>
#include "SpriteSet.h"
#include "TextLayout.h"

class TextRenderer final {
	private:
		struct GlyphData {
			SpriteSet::Handle handle;
			int xOffset, yOffset, xAdvance;
		};

		std::unique_ptr<Shader> shader;
		int shaderMatrixUniformIndex;

		SpriteSet spriteSet;
		std::unordered_map<std::uint64_t, GlyphData> spriteMap;
		std::vector<unsigned> renderIndices;
	public:
		TextRenderer();
		void tick();
		void syncToGpu();
		// Pixel perfect renders glyphs at their fractional horizontal offsets and places the rasterizations at
		// integral pixel positions.
		void prepareForRendering(const TextLayout &layout, bool pixelPerfect);
		void renderText(const TextLayout &layout, const glm::mat4 &projectionMatrix, bool pixelPerfect);
};

#endif // TEXT_FONTRENDERER_H