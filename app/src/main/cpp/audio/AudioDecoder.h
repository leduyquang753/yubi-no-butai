#ifndef YUBINOBUTAI_AUDIODECODER_H
#define YUBINOBUTAI_AUDIODECODER_H

#include <cstdint>
#include <memory>
#include <string>

#include <android/asset_manager.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

class AudioDecoder final {
	private:
		static int readFileData(void *userPointer, std::uint8_t *buffer, int bufferSize);
		static std::int64_t seekFileData(void *userPointer, std::int64_t offset, int from);

		AAsset *audioAsset;

		std::unique_ptr<AVIOContext, void(*)(AVIOContext*)> avioContext;
		std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> avformatContext;
		std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)> avcodecContext;
		std::unique_ptr<SwrContext, void(*)(SwrContext*)> swrContext;
		std::unique_ptr<AVPacket, void(*)(AVPacket*)> avPacket;
		std::unique_ptr<AVFrame, void(*)(AVFrame*)> avFrame;
		std::unique_ptr<std::uint8_t, void(*)(std::uint8_t*)> avSamples;
		int avSamplesSize = 0;
		AVStream *avStream;
	public:
		AudioDecoder(AAssetManager *assetManager, const std::string &name);
		~AudioDecoder();
		int decodeOneChunk();
		void retrieveAudio(float *buffer, int frameCount);
};

#endif // YUBINOBUTAI_AUDIODECODER_H