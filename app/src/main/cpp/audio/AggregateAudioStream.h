#ifndef YUBINOBUTAI_AGGREGATEAUDIOSTREAM_H
#define YUBINOBUTAI_AGGREGATEAUDIOSTREAM_H

#include <mutex>
#include <vector>

#include "AudioStream.h"

class AggregateAudioStream final: public AudioStream {
	private:
		struct InternalHandle {
			int playingIndex;
			unsigned long nonce;
			int nextFreeId;
		};
		struct PlayingStream {
			int id;
			AudioStream *stream;
		};

		std::vector<InternalHandle> handles;
		int nextFreeHandleId = 0;
		unsigned long nextNonce = 0;
		std::vector<PlayingStream> playingStreams;
		std::vector<float> streamBuffer;

		std::mutex mutex;
	public:
		struct Handle {
			int id;
			unsigned long nonce;
		};

		AggregateAudioStream(int maxStreams = 100);
		~AggregateAudioStream();
		int getAudio(float *&buffer, int frameCount) override;
		Handle play(AudioStream *stream);
		bool isPlaying(Handle handle);
		void stop(Handle handle);
};

#endif // YUBINOBUTAI_AGGREGATEAUDIOSTREAM_H