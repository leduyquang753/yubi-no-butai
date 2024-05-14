#ifndef YUBINOBUTAI_PRELOADEDAUDIOSTREAM_H
#define YUBINOBUTAI_PRELOADEDAUDIOSTREAM_H

#include "AudioStream.h"

class PreloadedAudioTrack;

class PreloadedAudioStream final: public AudioStream {
	private:
		PreloadedAudioTrack *audioTrack;
		int currentPosition = 0;
	public:
		PreloadedAudioStream(PreloadedAudioTrack &audioTrack): audioTrack(&audioTrack) {}
		int getAudio(float *&buffer, int frameCount) override;
};

#endif // YUBINOBUTAI_PRELOADEDAUDIOSTREAM_H