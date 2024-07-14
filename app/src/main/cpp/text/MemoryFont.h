#ifndef TEXT_MEMORYFONT_H
#define TEXT_MEMORYFONT_H

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <android/asset_manager.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <minikin/Buffer.h>
#include <minikin/Font.h>
#include <minikin/FontVariation.h>
#include <minikin/MinikinExtent.h>
#include <minikin/MinikinFont.h>
#include <minikin/MinikinPaint.h>
#include <minikin/MinikinRect.h>

class MemoryFont final: public minikin::MinikinFont {
	private:
		static const std::string emptyPath;
		static const std::vector<minikin::FontVariation> noVariations;

		AAsset *asset;
	public:
		std::uint64_t id;
		int index;
		FT_Library freeType;
		FT_Face fontFace;

		MemoryFont(AAssetManager *assetManager, std::uint64_t id, std::string path, int index);
		~MemoryFont() override;
		const std::string& GetFontPath() const override {
			return emptyPath;
		}
		std::size_t GetFontSize() const override {
			return AAsset_getLength(asset);
		}
		int GetFontIndex() const override {
			return index;
		}
		const std::vector<minikin::FontVariation>& GetAxes() const override {
			return noVariations;
		}
		const void* GetFontData() const override {
			return AAsset_getBuffer(asset);
		}

		float GetHorizontalAdvance(
			std::uint32_t glyphId, const minikin::MinikinPaint &paint, const minikin::FontFakery &fakery
		) const override;
		void GetBounds(
			minikin::MinikinRect *bounds, std::uint32_t glyphId,
			const minikin::MinikinPaint &paint, const minikin::FontFakery &fakery
		) const override;
		void GetFontExtent(
			minikin::MinikinExtent *extent, const minikin::MinikinPaint &paint, const minikin::FontFakery &fakery
		) const override;
};

#endif // TEXT_MEMORYFONT_H