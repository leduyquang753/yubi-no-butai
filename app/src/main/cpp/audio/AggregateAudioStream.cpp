#include <algorithm>
#include <mutex>

#include "AudioStream.h"

#include "AggregateAudioStream.h"

AggregateAudioStream::AggregateAudioStream(const int maxStreams): handles(maxStreams) {
	playingStreams.reserve(maxStreams);
	const int lastIndex = maxStreams - 1;
	for (int i = 0; i != lastIndex; ++i) handles[i].nextFreeId = i + 1;
	handles[lastIndex].nextFreeId = -1;
}

int AggregateAudioStream::getAudio(float *&buffer, const int frameCount) {
	const std::lock_guard<std::mutex> lock(mutex);
	const int sampleCount = frameCount * 2;
	std::fill(buffer, buffer + sampleCount, 0.f);
	streamBuffer.resize(sampleCount);
	int newPlayingCount = playingStreams.size();
	for (int i = 0; i != newPlayingCount;) {
		auto &playingStream = playingStreams[i];
		bool finished = true;
		if (playingStream.stream != nullptr) {
			float *streamBufferPointer = streamBuffer.data();
			const int streamFrameCount = playingStream.stream->getAudio(streamBufferPointer, frameCount);
			const int streamSampleCount = streamFrameCount * 2;
			for (int sample = 0; sample != streamSampleCount; ++sample) buffer[sample] += streamBufferPointer[sample];
			finished = streamFrameCount < frameCount;
		}
		if (finished) {
			const int id = playingStream.id;
			InternalHandle &handle = handles[id];
			handle.playingIndex = -1;
			handle.nextFreeId = nextFreeHandleId;
			nextFreeHandleId = id;
			--newPlayingCount;
			if (i != newPlayingCount) {
				playingStream = playingStreams[newPlayingCount];
				handles[playingStream.id].playingIndex = i;
			}
		} else {
			++i;
		}
	}
	playingStreams.resize(newPlayingCount);
	return frameCount;
}

AggregateAudioStream::Handle AggregateAudioStream::play(AudioStream *const stream) {
	const std::lock_guard<std::mutex> lock(mutex);
	if (nextFreeHandleId == -1) return {-1, 0};
	const int id = nextFreeHandleId;
	InternalHandle &handle = handles[id];
	handle.playingIndex = playingStreams.size();
	handle.nonce = nextNonce;
	++nextNonce;
	playingStreams.push_back({id, stream});
	nextFreeHandleId = handle.nextFreeId;
	return {id, handle.nonce};
}

bool AggregateAudioStream::isPlaying(const Handle handle) {
	if (handle.id == -1) return false;
	const InternalHandle &internalHandle = handles[handle.id];
	return internalHandle.nonce == handle.nonce && internalHandle.playingIndex != -1;
}

void AggregateAudioStream::stop(const Handle handle) {
	if (handle.id == -1) return;
	const std::lock_guard<std::mutex> lock(mutex);
	const InternalHandle &internalHandle = handles[handle.id];
	if (internalHandle.nonce != handle.nonce) return;
	const int index = internalHandle.playingIndex;
	if (index != -1) playingStreams[index].stream = nullptr;
}

AggregateAudioStream::~AggregateAudioStream() {}