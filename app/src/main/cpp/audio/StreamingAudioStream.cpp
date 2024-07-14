#include <algorithm>
#include <string>

#include "AudioDecodingThread.h"

#include "StreamingAudioStream.h"

#include <android/asset_manager.h>

namespace {
	constexpr int minimumLoadBufferLength = 48000 * 2;
	constexpr int loadBufferSize = minimumLoadBufferLength * 3;
} // namespace

StreamingAudioStream::Internal::Internal(
	AAssetManager *const assetManager, const std::string &name, AudioDecodingThread &audioDecodingThread
): audioDecodingThread(&audioDecodingThread), audioDecoder(assetManager, name) {
	chunk1.buffer.reserve(loadBufferSize);
	chunk2.buffer.reserve(loadBufferSize);
	chunk1.hasNext = true;
	audioDecodingThread.addTask({this, false});
}

void StreamingAudioStream::Internal::fill() {
	Chunk &chunk = playingChunk2 ? chunk1 : chunk2;
	chunk.buffer.clear();
	chunk.hasNext = true;
	while (chunk.buffer.size() < minimumLoadBufferLength) {
		const int chunkFrameCount = audioDecoder.decodeOneChunk();
		if (chunkFrameCount == 0) {
			chunk.hasNext = false;
			break;
		}
		const int currentChunkSize = static_cast<int>(chunk.buffer.size());
		chunk.buffer.resize(currentChunkSize + chunkFrameCount * 2);
		audioDecoder.retrieveAudio(chunk.buffer.data() + currentChunkSize, chunkFrameCount);
	}
	decodingNextChunk = false;
}

int StreamingAudioStream::Internal::getAudio(float *&buffer, int frameCount) {
	const int sampleCount = frameCount * 2;
	int servedSampleCount = 0;
	while (true) {
		Chunk &currentChunk = playingChunk2 ? chunk2 : chunk1;
		const int availableSampleCount = std::min(
			static_cast<int>(currentChunk.buffer.size()) - currentChunkPosition, sampleCount - servedSampleCount
		);
		std::copy(
			currentChunk.buffer.begin() + currentChunkPosition,
			currentChunk.buffer.begin() + (currentChunkPosition + availableSampleCount),
			buffer + servedSampleCount
		);
		servedSampleCount += availableSampleCount;
		currentChunkPosition += availableSampleCount;
		currentPosition += availableSampleCount / 2;
		if (servedSampleCount == sampleCount) return frameCount;
		if (!currentChunk.hasNext) return servedSampleCount / 2;
		if (decodingNextChunk) {
			// Decoding couldn't keep up. Temporarily serve silence.
			std::fill(buffer + servedSampleCount, buffer + sampleCount, 0.f);
			return frameCount;
		} else {
			playingChunk2 = !playingChunk2;
			currentChunkPosition = 0;
			decodingNextChunk = true;
			audioDecodingThread->addTask({this, false});
		}
	}
}

void StreamingAudioStream::Internal::queueDestruction() {
	audioDecodingThread->addTask({this, true});
}