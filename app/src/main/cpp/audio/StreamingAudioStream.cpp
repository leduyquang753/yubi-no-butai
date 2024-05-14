#include <algorithm>
#include <string>

#include "StreamingAudioStream.h"

#include <android/asset_manager.h>

namespace {
	constexpr int minimumLoadBufferLength = 4800;
	constexpr int loadBufferSize = minimumLoadBufferLength * 3;
} // namespace

StreamingAudioStream::StreamingAudioStream(
	AAssetManager *const assetManager, const std::string &name
): audioDecoder(assetManager, name) {
	loadBuffer.reserve(loadBufferSize);
}

int StreamingAudioStream::getAudio(float *&buffer, int frameCount) {
	const int positionInBuffer = currentPosition - loadBufferPosition;
	if ((positionInBuffer + frameCount) * 2 <= loadBuffer.size()) {
		buffer = loadBuffer.data() + positionInBuffer * 2;
		currentPosition += frameCount;
		return frameCount;
	} else if (loadFinished) {
		const int actualFrameCount = static_cast<int>(loadBuffer.size()) / 2 - positionInBuffer;
		buffer = loadBuffer.data() + positionInBuffer * 2;
		currentPosition += actualFrameCount;
		return actualFrameCount;
	} else {
		std::copy(loadBuffer.begin() + positionInBuffer * 2, loadBuffer.end(), buffer);
		const int loadedFrameCount = static_cast<int>(loadBuffer.size()) / 2 - positionInBuffer;
		currentPosition += loadedFrameCount;
		loadBufferPosition = currentPosition;
		loadBuffer.clear();
		while (true) {
			const int chunkFrameCount = audioDecoder.decodeOneChunk();
			if (chunkFrameCount == 0) {
				loadFinished = true;
				const int framesRemaining = std::min(
					frameCount - loadedFrameCount, static_cast<int>(loadBuffer.size()) / 2
				);
				std::copy(loadBuffer.begin(), loadBuffer.begin() + framesRemaining * 2, buffer + loadedFrameCount * 2);
				currentPosition += framesRemaining;
				return framesRemaining;
			}
			const int currentNewlyLoadedFrameCount = static_cast<int>(loadBuffer.size());
			loadBuffer.resize(currentNewlyLoadedFrameCount + chunkFrameCount * 2);
			audioDecoder.retrieveAudio(loadBuffer.data() + currentNewlyLoadedFrameCount, chunkFrameCount);
			if (loadBuffer.size() >= minimumLoadBufferLength * 2) {
				const int framesRemaining = frameCount - loadedFrameCount;
				std::copy(
					loadBuffer.begin(), loadBuffer.begin() + framesRemaining * 2, buffer + loadedFrameCount * 2
				);
				currentPosition += framesRemaining;
				return frameCount;
			}
		}
	}
}