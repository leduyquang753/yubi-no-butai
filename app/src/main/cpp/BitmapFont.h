#ifndef YUBINOBUTAI_BITMAPFONT_H
#define YUBINOBUTAI_BITMAPFONT_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <glm/glm.hpp>

#include "BasicData.h"
#include "Shader.h"
#include "TextureAsset.h"

class BitmapFont {
	private:
		struct Glyph {
			int width, height, offsetX, offsetY, advance;
			float textureTop, textureBottom, textureLeft, textureRight;
		};
		struct RenderingVertex {
			Vector2 position;
			Vector2 uv;
		};

		std::shared_ptr<TextureAsset> texture;
		std::unique_ptr<Shader> shader;
		GLint uProjection, uColor, inPosition, inUv;

		int baseSize, lineHeight, baseHeight, textureWidth, textureHeight;
		std::unordered_map<char32_t, Glyph> glyphs;

		const Glyph& getGlyph(char32_t character) const;
	public:
		struct MeasureResult {
			float width, height, heightFromBaseline;
		};

		BitmapFont(AAssetManager *const assetManager, const std::string &name);
		MeasureResult measure(std::string_view text, float size) const;
		void render(std::string_view text, float size, const glm::mat4 &matrix, Vector4 color) const;
};

#endif // YUBINOBUTAI_BITMAPFONT_H