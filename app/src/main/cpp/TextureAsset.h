#ifndef YUBINOBUTAI_TEXTUREASSET_H
#define YUBINOBUTAI_TEXTUREASSET_H

#include <memory>
#include <string>
#include <vector>

#include <android/asset_manager.h>
#include <GLES3/gl3.h>

class TextureAsset {
	private:
		GLuint textureId;

		TextureAsset(GLuint textureId): textureId(textureId) {}
	public:
		static std::shared_ptr<TextureAsset> loadAsset(AAssetManager *assetManager, const std::string &assetPath);

		~TextureAsset();
		constexpr GLuint getTextureId() const {
			return textureId;
		}
};

#endif // YUBINOBUTAI_TEXTUREASSET_H