#ifndef YUBINOBUTAI_AUDIOSTREAM_H
#define YUBINOBUTAI_AUDIOSTREAM_H

class AudioStream {
	public:
		virtual ~AudioStream() = 0;
		virtual int getAudio(float *&buffer, int frameCount) = 0;
};

inline AudioStream::~AudioStream() {}

#endif // YUBINOBUTAI_AUDIOSTREAM_H