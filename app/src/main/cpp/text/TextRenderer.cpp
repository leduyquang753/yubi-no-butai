#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <GLES3/gl3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "MemoryFont.h"
#include "TextLayout.h"

#include "TextRenderer.h"

namespace {
const char *const textVertexShaderSource = R"shader(#version 300 es
uniform mat4 projection;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;

out vec2 fragUv;
flat out vec4 fragColor;

void main() {
	gl_Position = projection * vec4(position.xy, 0.f, 1.f);
	fragUv = uv / 1024.f;
	fragColor = color;
}
)shader";

const char *const textFragmentShaderSource = R"shader(#version 300 es
precision highp float;

uniform sampler2D sprite;

in vec2 fragUv;
flat in vec4 fragColor;

out vec4 color;

void main() {
	color = vec4(fragColor.rgb, texture(sprite, vec2(fragUv)) * fragColor.a);
}
)shader";
}

namespace {
	struct Batch {
		bool used;
		unsigned textureId;
		std::vector<float> data;
	};

	std::uint64_t makeGlyphKey(
		const std::uint16_t fontId, const float size, const std::uint32_t glyphId,
		const float offset
	) {
		return (static_cast<std::uint64_t>(fontId) << 48)
			| (static_cast<std::uint64_t>(size * 4.f) << 34)
			| (static_cast<std::uint64_t>(glyphId) << 2)
			| (static_cast<std::uint64_t>(offset * 4.f) & 0b11);
	}

	std::vector<Batch> batches;
}

TextRenderer::TextRenderer():
	shader(std::move(Shader::loadShader(textVertexShaderSource, textFragmentShaderSource))),
	spriteSet(1)
{
	const auto shaderProgram = shader->getProgram();
	shader->activate();
	shaderMatrixUniformIndex = glGetUniformLocation(shaderProgram, "projection");
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
}

void TextRenderer::tick() {
	spriteSet.tick();
}

void TextRenderer::syncToGpu() {
	spriteSet.syncToGpu();
}

void TextRenderer::prepareForRendering(const TextLayout &layout, const bool pixelPerfect) {
	const auto &text = layout.getText();
	for (const auto &line : layout.getLines()) for (const auto &run : line.runs) {
		const auto fontScale = static_cast<FT_F26Dot6>(run.size * 64.f);
		const auto &runLayout = run.layout;
		const int glyphCount = static_cast<int>(runLayout.nGlyphs());
		for (int i = 0; i != glyphCount; ++i) {
			const auto &font = static_cast<MemoryFont&>(*runLayout.getFont(i)->baseTypeface());
			const auto fontFace = font.fontFace;
			const unsigned long c = runLayout.getGlyphId(i);
			const float characterX = runLayout.getX(i);
			const auto glyphKey = makeGlyphKey(
				static_cast<std::uint16_t>(font.id), run.size, c, pixelPerfect ? characterX : 0.f
			);
			const auto mapIterator = spriteMap.find(glyphKey);
			if (mapIterator != spriteMap.end() && spriteSet.ping(mapIterator->second.handle)) continue;
			FT_Set_Char_Size(fontFace, fontScale, fontScale, 72, 72);
			FT_Load_Glyph(fontFace, c, 0);
			FT_Glyph glyph;
			FT_Get_Glyph(fontFace->glyph, &glyph);
			FT_Vector offset = {(static_cast<int>(characterX * 4.f) & 0b11) << 4, 0};
			FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, &offset, true);
			const auto &bitmapGlyph = *reinterpret_cast<FT_BitmapGlyph>(glyph);
			const auto &bitmap = bitmapGlyph.bitmap;
			const int xAdvance = bitmapGlyph.root.advance.x >> 10;
			spriteMap.emplace(glyphKey, GlyphData{
				spriteSet.add(bitmap.width, bitmap.rows, bitmap.buffer),
				bitmapGlyph.left, -bitmapGlyph.top, xAdvance
			});
			FT_Done_Glyph(glyph);
		}
	}
}

void TextRenderer::renderText(const TextLayout &layout, const glm::mat4 &projectionMatrix, const bool pixelPerfect) {
	const auto &text = layout.getText();
	for (auto &batch : batches) {
		if (!batch.used) break;
		batch.used = false;
		batch.data.clear();
	}
	auto currentBatch = batches.end();
	float currentY = 0.f;
	for (const auto &line : layout.getLines()) {
		const float lineY = currentY + line.ascent;
		currentY += line.ascent + line.descent;
		float currentX = line.indent;
		for (const auto &run : line.runs) {
			const auto &runLayout = run.layout;
			const std::size_t glyphCount = runLayout.nGlyphs();
			for (std::size_t i = 0; i != glyphCount; ++i) {
				const auto &glyphData = spriteMap[makeGlyphKey(
					static_cast<std::uint16_t>(static_cast<MemoryFont&>(*runLayout.getFont(i)->baseTypeface()).id),
					run.size, runLayout.getGlyphId(i), pixelPerfect ? runLayout.getX(i) : 0.f
				)];
				const auto spriteData = spriteSet.get(glyphData.handle);
				const float
					characterX = currentX + glyphData.xOffset
						+ (pixelPerfect ? std::floor(runLayout.getX(i)) : runLayout.getX(i)),
					characterY = lineY + glyphData.yOffset
						+ (pixelPerfect ? std::floor(runLayout.getY(i)) : runLayout.getY(i));
				const float vertexData[] = {
					characterX, characterY,
					static_cast<float>(spriteData.x), static_cast<float>(spriteData.y),
					run.red, run.green, run.blue, run.alpha,

					characterX, characterY + spriteData.height,
					static_cast<float>(spriteData.x), static_cast<float>(spriteData.y + spriteData.height),
					run.red, run.green, run.blue, run.alpha,

					characterX + spriteData.width, characterY + spriteData.height,
					static_cast<float>(spriteData.x + spriteData.width),
					static_cast<float>(spriteData.y + spriteData.height),
					run.red, run.green, run.blue, run.alpha,

					characterX + spriteData.width, characterY,
					static_cast<float>(spriteData.x + spriteData.width), static_cast<float>(spriteData.y),
					run.red, run.green, run.blue, run.alpha
				};
				if (currentBatch == batches.end() || currentBatch->textureId != spriteData.textureId) {
					currentBatch = std::find_if(batches.begin(), batches.end(), [&spriteData](const auto &batch) {
						return !batch.used || batch.textureId == spriteData.textureId;
					});
					if (currentBatch == batches.end()) {
						batches.push_back({true, spriteData.textureId});
						currentBatch = batches.end() - 1;
					} else {
						currentBatch->used = true;
					}
				}
				currentBatch->data.insert(currentBatch->data.end(), vertexData, vertexData + 32);
			}
			currentX += runLayout.getAdvance();
		}
	}
	shader->activate();
	glUniformMatrix4fv(shaderMatrixUniformIndex, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	for (const Batch &batch : batches) {
		const unsigned quadCount = static_cast<unsigned>(batch.data.size()) / 32;
		for (unsigned i = static_cast<unsigned>(renderIndices.size()) / 6; i != quadCount; ++i) {
			const unsigned currentIndex = i * 4;
			const unsigned newIndices[] = {
				currentIndex, currentIndex + 1, currentIndex + 2,
				currentIndex + 2, currentIndex + 3, currentIndex
			};
			renderIndices.insert(renderIndices.end(), newIndices, newIndices + 6);
		}
		glBindTexture(GL_TEXTURE_2D, batch.textureId);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), batch.data.data());
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), batch.data.data() + 2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8*sizeof(float), batch.data.data() + 4);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quadCount * 6), GL_UNSIGNED_INT, renderIndices.data());
	}
}