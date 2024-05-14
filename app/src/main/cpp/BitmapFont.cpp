#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "BasicData.h"
#include "Shader.h"
#include "TextureAsset.h"

#include "BitmapFont.h"

namespace {

const std::string vertexShader = R"vertex(
#version 300 es
uniform mat4 uProjection;
in vec2 inPosition;
in vec2 inUv;
out vec2 fragUv;

void main() {
	gl_Position = uProjection * vec4(inPosition, 0.0, 1.0);
	fragUv = inUv;
}
)vertex";

static const std::string fragmentShader = R"fragment(
#version 300 es
precision mediump float;
uniform sampler2D uTexture;
uniform vec4 uColor;
in vec2 fragUv;
out vec4 outColor;

void main() {
	outColor = vec4(uColor.rgb, texture(uTexture, fragUv).r * uColor.a);
}
)fragment";

std::int64_t readInteger(const char *const buffer, const int bytes, const bool hasSignBit) {
	std::int64_t number = 0;
	for (int pos = bytes-1; pos != -1; --pos)
		number = (number << 8) | static_cast<std::uint8_t>(buffer[pos]);
	return hasSignBit && (number & 0x8000) ? number - 0x10000 : number;
}

} // namespace

BitmapFont::BitmapFont(AAssetManager *const assetManager, const std::string &name):
	texture(std::move(TextureAsset::loadAsset(assetManager, name + "_0.png"))),
	shader(std::move(Shader::loadShader(vertexShader, fragmentShader)))
{
	const auto shaderProgram = shader->getProgram();
	uProjection = glGetUniformLocation(shaderProgram, "uProjection");
	uColor = glGetUniformLocation(shaderProgram, "uColor");
	inPosition = glGetAttribLocation(shaderProgram, "inPosition");
	inUv = glGetAttribLocation(shaderProgram, "inUv");

	const auto descriptorAsset = AAssetManager_open(assetManager, (name + ".fnt").c_str(), AASSET_MODE_BUFFER);
	const char *dataPointer = static_cast<const char*>(AAsset_getBuffer(descriptorAsset));
	int blockSize;

	const auto readSint = [&dataPointer]() {
		const auto value = readInteger(dataPointer, 2, true);
		dataPointer += 2;
		return static_cast<int>(value);
	};
	const auto readUint = [&dataPointer](const int size) {
		const auto value = readInteger(dataPointer, size, false);
		dataPointer += size;
		return value;
	};
	const auto readBlockSize = [&dataPointer, &blockSize, &readUint]() {
		dataPointer += 1;
		blockSize = static_cast<int>(readUint(4));
	};

	// File format according to
	// https://www.angelcode.com/products/bmfont/doc/file_format.html#bin

	dataPointer += 4; // Header and version.

	// Block 1: Font information.
	readBlockSize();
	baseSize = readSint();
	dataPointer += blockSize - 2;

	// Block 2: Font parameters.
	readBlockSize();
	lineHeight = static_cast<int>(readUint(2));
	baseHeight = static_cast<int>(readUint(2));
	textureWidth = static_cast<int>(readUint(2));
	textureHeight = static_cast<int>(readUint(2));
	dataPointer += 7;

	// Block 3: Page file names.
	// Skip this as the textures files have been post-processed.
	readBlockSize();
	dataPointer += blockSize;

	// Block 4: Characters.
	readBlockSize();
	const int characterCount = blockSize / 20;
	for (int i = 0; i != characterCount; ++i) {
		const char32_t character = static_cast<char32_t>(readUint(4));
		Glyph &glyph = glyphs[character];
		const unsigned int x = static_cast<unsigned int>(readUint(2));
		const unsigned int y = static_cast<unsigned int>(readUint(2));
		glyph.width = static_cast<int>(readUint(2));
		glyph.height = static_cast<int>(readUint(2));
		glyph.offsetX = readSint();
		glyph.offsetY = baseHeight - readSint();
		glyph.advance = readSint();
		glyph.textureTop = static_cast<float>(y) / textureWidth;
		glyph.textureBottom = static_cast<float>(y + glyph.height) / textureHeight;
		glyph.textureLeft = static_cast<float>(x) / textureWidth;
		glyph.textureRight = static_cast<float>(x + glyph.width) / textureWidth;
		dataPointer += 2;
	}

	AAsset_close(descriptorAsset);
}

const BitmapFont::Glyph& BitmapFont::getGlyph(const char32_t character) const {
	const auto glyphIter = glyphs.find(character);
	return glyphIter == glyphs.end() ? glyphs.at(-1) : glyphIter->second;
}

BitmapFont::MeasureResult BitmapFont::measure(const std::string_view text, const float size) const {
	bool isFirst = true;
	int width = 0, maxTop = 0, minBottom = 0;
	for (const char character : text) {
		const auto &glyph = getGlyph(character);
		width += glyph.advance;
		const int
			top = glyph.offsetY,
			bottom = top - glyph.height;
		if (isFirst) {
			maxTop = top;
			minBottom = bottom;
			isFirst = false;
		} else {
			if (top > maxTop) maxTop = top;
			if (bottom < minBottom) minBottom = bottom;
		}
	}
	const float scale = size / baseSize;
	return {width * scale, (maxTop - minBottom) * scale, maxTop * scale};
}

void BitmapFont::render(
	const std::string_view text, const float size, const glm::mat4 &matrix, const Vector4 color
) const {
	shader->activate();

	const float scale = size / baseSize;
	const auto scaledMatrix = glm::scale(matrix, glm::vec3(scale, scale, scale));
	glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(scaledMatrix));
	glUniform4fv(uColor, 1, color.index);

	const int glyphCount = static_cast<int>(text.size());
	std::vector<RenderingVertex> vertices(glyphCount * 4);
	float x = 0.f;
	auto vertexPointer = vertices.begin();
	for (const char character : text) {
		const auto &glyph = getGlyph(character);
		const float
			top = glyph.offsetY,
			bottom = top - glyph.height,
			left = x + glyph.offsetX,
			right = left + glyph.width;
		vertexPointer[0] = {right, top, glyph.textureRight, glyph.textureTop};
		vertexPointer[1] = {left, top, glyph.textureLeft, glyph.textureTop};
		vertexPointer[2] = {left, bottom, glyph.textureLeft, glyph.textureBottom};
		vertexPointer[3] = {right, bottom, glyph.textureRight, glyph.textureBottom};
		vertexPointer += 4;
		x += glyph.advance;
	}
	std::vector<Index> indices(glyphCount * 6);
	for (int i = 0; i != glyphCount; ++i) {
		const int baseIndex = i * 4;
		const auto indexPointer = indices.begin() + i * 6;
		indexPointer[0] = static_cast<Index>(baseIndex);
		indexPointer[1] = static_cast<Index>(baseIndex + 1);
		indexPointer[2] = static_cast<Index>(baseIndex + 2);
		indexPointer[3] = static_cast<Index>(baseIndex + 2);
		indexPointer[4] = static_cast<Index>(baseIndex + 3);
		indexPointer[5] = static_cast<Index>(baseIndex);
	}

	glEnableVertexAttribArray(inPosition);
	glEnableVertexAttribArray(inUv);
	glVertexAttribPointer(
		inPosition, 2, GL_FLOAT, GL_FALSE, sizeof(RenderingVertex), vertices[0].position.index
	);
	glVertexAttribPointer(
		inUv, 2, GL_FLOAT, GL_FALSE, sizeof(RenderingVertex), vertices[0].uv.index
	);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->getTextureId());
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_SHORT, indices.data());

	glDisableVertexAttribArray(inPosition);
	glDisableVertexAttribArray(inUv);
}