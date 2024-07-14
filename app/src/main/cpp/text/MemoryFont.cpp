#include <cstdint>
#include <string>
#include <vector>

#include <android/asset_manager.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <minikin/Font.h>
#include <minikin/FontVariation.h>
#include <minikin/MinikinExtent.h>
#include <minikin/MinikinPaint.h>
#include <minikin/MinikinRect.h>

#include "MemoryFont.h"

namespace {
	constexpr float FtPosToFloat(const FT_Pos position) {
		return position / 64.f;
	}

	constexpr FT_F26Dot6 FtFloatToF26Dot6(const float value) {
		return static_cast<FT_F26Dot6>(value * 64.f);
	}

	void loadGlyph(const FT_Face fontFace, const float size, const std::uint32_t glyphId) {
		const auto scale = FtFloatToF26Dot6(size);
		FT_Set_Char_Size(fontFace, scale, scale, 72, 72);
		FT_Load_Glyph(fontFace, glyphId, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
	}
}

const std::string MemoryFont::emptyPath;
const std::vector<minikin::FontVariation> MemoryFont::noVariations;

MemoryFont::MemoryFont(
	AAssetManager *const assetManager, const std::uint64_t id, const std::string path, const int index
):
	id(id), index(index),
	asset(AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_BUFFER))
{
	FT_Init_FreeType(&freeType);
	FT_Open_Args fontOpenArgs;
	fontOpenArgs.flags = FT_OPEN_MEMORY;
	fontOpenArgs.memory_base = static_cast<const FT_Byte*>(AAsset_getBuffer(asset));
	fontOpenArgs.memory_size = AAsset_getLength(asset);
	FT_Open_Face(freeType, &fontOpenArgs, index, &fontFace);
}

float MemoryFont::GetHorizontalAdvance(
	const std::uint32_t glyphId, const minikin::MinikinPaint &paint, const minikin::FontFakery &fakery
) const {
	loadGlyph(fontFace, paint.size, glyphId);
	return FtPosToFloat(fontFace->glyph->advance.x);
}

void MemoryFont::GetBounds(
	minikin::MinikinRect *const bounds, const std::uint32_t glyphId,
	const minikin::MinikinPaint &paint, const minikin::FontFakery &fakery
) const {
	loadGlyph(fontFace, paint.size, glyphId);
	FT_BBox boundingBox;
	FT_Outline_Get_CBox(&fontFace->glyph->outline, &boundingBox);
	bounds->mLeft = FtPosToFloat(boundingBox.xMin);
	bounds->mTop = FtPosToFloat(boundingBox.yMax);
	bounds->mRight = FtPosToFloat(boundingBox.xMax);
	bounds->mBottom = FtPosToFloat(boundingBox.yMin);
}

void MemoryFont::GetFontExtent(
	minikin::MinikinExtent *const extent, const minikin::MinikinPaint &paint, const minikin::FontFakery &fakery
) const {
	float scale = -(paint.size / fontFace->units_per_EM);
	extent->ascent = fontFace->ascender * scale;
	extent->descent = fontFace->descender * scale;
}

MemoryFont::~MemoryFont() {
	FT_Done_Face(fontFace);
	FT_Done_FreeType(freeType);
	AAsset_close(asset);
}