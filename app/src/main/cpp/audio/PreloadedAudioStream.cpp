#include <algorithm>

#include "PreloadedAudioTrack.h"

#include "PreloadedAudioStream.h"

int PreloadedAudioStream::getAudio(float *&buffer, int frameCount) {
	// Don't worry, the data will never be written to.
	buffer = const_cast<float*>(audioTrack->getAudioData().data()) + currentPosition * 2;
	const int actualFrameCount = std::min(frameCount, audioTrack->getLength() - currentPosition);
	currentPosition += actualFrameCount;
	return actualFrameCount;
}