#ifndef YUBINOBUTAI_STEAMINGAUDIOSTREAM_H
#define YUBINOBUTAI_STEAMINGAUDIOSTREAM_H

#include <atomic>
#include <string>
#include <vector>

#include <android/asset_manager.h>

#include "AudioDecoder.h"
#include "AudioStream.h"

class AudioDecodingThread;

class StreamingAudioStream final: public AudioStream {
	public:
		class Internal final {
			private:
				struct Chunk {
					std::vector<float> buffer;
					bool hasNext;
				};

				AudioDecodingThread *audioDecodingThread;
				AudioDecoder audioDecoder;
				Chunk chunk1, chunk2;
				bool playingChunk2 = false;
				int currentChunkPosition = 0;
				int currentPosition = 0;
				std::atomic_bool decodingNextChunk = true;
			public:
				Internal(
					AAssetManager *assetManager, const std::string &name, AudioDecodingThread &audioDecodingThread
				);
				void fill();
				int getAudio(float *&buffer, int frameCount);
				bool isReadyToPlay() const {
					return !decodingNextChunk;
				}
				double getTime() const {
					return currentPosition / 48.;
				}
				void queueDestruction();
		};
	private:
		Internal *internal;
	public:
		StreamingAudioStream(
			AAssetManager *const assetManager, const std::string &name, AudioDecodingThread &audioDecodingThread
		): internal(new Internal(assetManager, name, audioDecodingThread)) {}
		~StreamingAudioStream() {
			internal->queueDestruction();
		}
		int getAudio(float *&buffer, const int frameCount) override {
			return internal->getAudio(buffer, frameCount);
		}
		bool isReadyToPlay() const {
			return internal->isReadyToPlay();
		}
		double getTime() const {
			return internal->getTime();
		}
};

#endif // YUBINOBUTAI_STEAMINGAUDIOSTREAM_H