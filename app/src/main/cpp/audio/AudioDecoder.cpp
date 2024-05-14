#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include <android/asset_manager.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "AudioDecoder.h"

namespace {
	constexpr int avioBufferSize = 4 << 10;
} // namespace

int AudioDecoder::readFileData(void *userPointer, std::uint8_t *buffer, int bufferSize) {
	const int result = AAsset_read(static_cast<AudioDecoder*>(userPointer)->audioAsset, buffer, bufferSize);
	return result == 0 ? -1 : result;
}

std::int64_t AudioDecoder::seekFileData(void *userPointer, std::int64_t offset, int from) {
	const auto audioAsset = static_cast<AudioDecoder*>(userPointer)->audioAsset;
	return from == AVSEEK_SIZE ? AAsset_getLength(audioAsset)
		: AAsset_seek(audioAsset, offset, from) == -1 ? -1
		: 0;
}

AudioDecoder::AudioDecoder(AAssetManager *const assetManager, const std::string &name):
	audioAsset(AAssetManager_open(assetManager, name.c_str(), AASSET_MODE_UNKNOWN)),
	avioContext(nullptr, [](AVIOContext *context) {
		av_free(context->buffer);
		avio_context_free(&context);
	}),
	avformatContext(nullptr, avformat_free_context),
	avcodecContext(nullptr, [](AVCodecContext *context) { avcodec_free_context(&context); }),
	swrContext(nullptr, [](SwrContext *context) { swr_free(&context); }),
	avPacket(nullptr, [](AVPacket *packet) { av_packet_free(&packet); }),
	avFrame(nullptr, [](AVFrame *frame) { av_frame_free(&frame); }),
	avSamples(nullptr, [](std::uint8_t *samples) { av_freep(&samples); })
{
	avioContext.reset(avio_alloc_context(
		static_cast<unsigned char*>(av_malloc(avioBufferSize)), avioBufferSize, false,
		this, readFileData, nullptr, seekFileData
	));
	avformatContext.reset(avformat_alloc_context());
	avformatContext->pb = avioContext.get();
	auto avformatContextRawPointer = avformatContext.get();
	avformat_open_input(&avformatContextRawPointer, "", nullptr, nullptr);
	avformat_find_stream_info(avformatContextRawPointer, nullptr);
	avStream = avformatContext->streams[av_find_best_stream(
		avformatContextRawPointer, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0
	)];
	const AVCodec *const avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
	avcodecContext.reset(avcodec_alloc_context3(avCodec));
	avcodec_parameters_to_context(avcodecContext.get(), avStream->codecpar);
	avcodec_open2(avcodecContext.get(), avCodec, nullptr);
	swrContext.reset(swr_alloc());
	AVChannelLayout avChannelLayout = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(swrContext.get(), "in_chlayout", &avStream->codecpar->ch_layout, 0);
    av_opt_set_chlayout(swrContext.get(), "out_chlayout", &avChannelLayout, 0);
    av_opt_set_int(swrContext.get(), "in_sample_rate", avStream->codecpar->sample_rate, 0);
    av_opt_set_int(swrContext.get(), "out_sample_rate", 48000, 0);
    av_opt_set_int(swrContext.get(), "in_sample_fmt", avStream->codecpar->format, 0);
    av_opt_set_sample_fmt(swrContext.get(), "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_int(swrContext.get(), "force_resampling", 1, 0);
    swr_init(swrContext.get());
    avPacket.reset(av_packet_alloc());
    avFrame.reset(av_frame_alloc());
}

int AudioDecoder::decodeOneChunk() {
	while (true) {
		if (av_read_frame(avformatContext.get(), avPacket.get()) != 0) return 0;
		if (avPacket->stream_index != avStream->index || avPacket->size == 0) continue;

		avcodec_send_packet(avcodecContext.get(), avPacket.get());
		avcodec_receive_frame(avcodecContext.get(), avFrame.get());
		const auto destinationFrameCount = av_rescale_rnd(
			swr_get_delay(swrContext.get(), avFrame->sample_rate) + avFrame->nb_samples,
			48000, avFrame->sample_rate, AV_ROUND_UP
		);
		if (destinationFrameCount > avSamplesSize) {
			avSamplesSize = destinationFrameCount * 2;
			std::uint8_t *avSamplesRawPointer;
			av_samples_alloc(&avSamplesRawPointer, nullptr, 2, avSamplesSize, AV_SAMPLE_FMT_FLT, 0);
			avSamples.reset(avSamplesRawPointer);
		}
		std::uint8_t *const avSamplesRawPointer = avSamples.get();
		const int convertedFrameCount = swr_convert(
			swrContext.get(), &avSamplesRawPointer, destinationFrameCount, avFrame->data, avFrame->nb_samples
		);
		av_packet_unref(avPacket.get());

		return convertedFrameCount;
	}
}

void AudioDecoder::retrieveAudio(float *const buffer, const int frameCount) {
	std::memcpy(buffer, avSamples.get(), sizeof(float) * 2 * frameCount);
}

AudioDecoder::~AudioDecoder() {
	AAsset_close(audioAsset);
}