#ifndef YUBINOBUTAI_STEAMINGAUDIOSTREAM_H
#define YUBINOBUTAI_STEAMINGAUDIOSTREAM_H

#include <string>
#include <vector>

#include <android/asset_manager.h>

#include "AudioDecoder.h"
#include "AudioStream.h"

class StreamingAudioStream final: public AudioStream {
	private:
		int currentPosition = 0;
		int loadBufferPosition = 0;
		bool loadFinished = false;
		std::vector<float> loadBuffer;
		AudioDecoder audioDecoder;
	public:
		StreamingAudioStream(AAssetManager *assetManager, const std::string &name);
		int getAudio(float *&buffer, int frameCount) override;
		double getTime() const {
			return currentPosition / 48.;
		}
};

#endif // YUBINOBUTAI_STEAMINGAUDIOSTREAM_H