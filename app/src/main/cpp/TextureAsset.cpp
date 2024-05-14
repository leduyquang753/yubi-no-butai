#include <android/asset_manager.h>
#include <android/imagedecoder.h>

#include "AndroidOut.h"
#include "TextureAsset.h"
#include "Utility.h"

std::shared_ptr<TextureAsset> TextureAsset::loadAsset(AAssetManager *const assetManager, const std::string &assetPath) {
	// Get the image from asset manager
	auto asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);

	// Make a decoder to turn it into a texture
	AImageDecoder *decoder = nullptr;
	auto result = AImageDecoder_createFromAAsset(asset, &decoder);
	assert(result == ANDROID_IMAGE_DECODER_SUCCESS);

	// make sure we get 8 bits per channel out. RGBA order.
	AImageDecoder_setAndroidBitmapFormat(decoder, ANDROID_BITMAP_FORMAT_RGBA_8888);

	// Get the image header, to help set everything up
	const AImageDecoderHeaderInfo *const header = AImageDecoder_getHeaderInfo(decoder);

	// important metrics for sending to GL
	auto width = AImageDecoderHeaderInfo_getWidth(header);
	auto height = AImageDecoderHeaderInfo_getHeight(header);
	auto stride = AImageDecoder_getMinimumStride(decoder);

	// Get the bitmap data of the image
	auto upAndroidImageData = std::make_unique<std::vector<uint8_t>>(height * stride);
	auto decodeResult = AImageDecoder_decodeImage(
		decoder, upAndroidImageData->data(), stride, upAndroidImageData->size()
	);
	assert(decodeResult == ANDROID_IMAGE_DECODER_SUCCESS);

	// Get an opengl texture
	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	// Clamp to the edge, you'll get odd results alpha blending if you don't
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load the texture into VRAM
	glTexImage2D(
		GL_TEXTURE_2D, // target
		0, // mip level
		GL_RGBA, // internal format, often advisable to use BGR
		width, // width of the texture
		height, // height of the texture
		0, // border (always 0)
		GL_RGBA, // format
		GL_UNSIGNED_BYTE, // type
		upAndroidImageData->data() // Data to upload
	);

	// generate mip levels. Not really needed for 2D, but good to do
	glGenerateMipmap(GL_TEXTURE_2D);

	// cleanup helpers
	AImageDecoder_delete(decoder);
	AAsset_close(asset);

	// Create a shared pointer so it can be cleaned up easily/automatically
	return std::shared_ptr<TextureAsset>(new TextureAsset(textureId));
}

TextureAsset::~TextureAsset() {
	if (textureId == 0) return;
	glDeleteTextures(1, &textureId);
	textureId = 0;
}