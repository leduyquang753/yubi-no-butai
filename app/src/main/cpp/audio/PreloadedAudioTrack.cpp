#include <string>

#include <android/asset_manager.h>

#include "AudioDecoder.h"

#include "PreloadedAudioTrack.h"

PreloadedAudioTrack::PreloadedAudioTrack(AAssetManager *const assetManager, const std::string &name) {
	AudioDecoder audioDecoder(assetManager, name);
	while (true) {
		const int chunkFrameCount = audioDecoder.decodeOneChunk();
		if (chunkFrameCount == 0) break;
		const auto currentSampleCount = audioData.size();
		audioData.resize(currentSampleCount + chunkFrameCount * 2);
		audioDecoder.retrieveAudio(audioData.data() + currentSampleCount, chunkFrameCount);
	}
	length = static_cast<int>(audioData.size()) / 2;
}