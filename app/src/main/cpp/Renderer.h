#ifndef YUBINOBUTAI_RENDERER_H
#define YUBINOBUTAI_RENDERER_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <EGL/egl.h>
#include <oboe/Oboe.h>

#include <audio/AggregateAudioStream.h>
#include <audio/PreloadedAudioStream.h>
#include <audio/PreloadedAudioTrack.h>
#include <audio/StreamingAudioStream.h>
#include "BitmapFont.h"
#include "Shader.h"
#include "TestLine.h"

struct android_app;

class Renderer: public oboe::AudioStreamDataCallback {
	private:
		void initRenderer();
		void updateRenderArea();

		android_app *appData;
		EGLDisplay display;
		EGLSurface surface;
		EGLContext context;
		EGLint width;
		EGLint height;

		std::optional<BitmapFont> font;
		std::optional<TestLine> testLine;

		std::shared_ptr<oboe::AudioStream> audioStream;
		std::unique_ptr<AggregateAudioStream> aggregateStream;
		std::unique_ptr<StreamingAudioStream> musicStream;
		std::unique_ptr<PreloadedAudioTrack> effectTrack;

		struct PlayingEffect {
			std::unique_ptr<PreloadedAudioStream> stream;
			AggregateAudioStream::Handle handle;
		};
		std::vector<PlayingEffect> playingEffects;

		struct Note {
			int time;
			int position;
			bool hit = false;
		};
		std::vector<Note> notes;
		std::vector<Note>::iterator nextNote;
		int hitCount = 0;
		double time = 0;

		void onTap(float pointerX, float pointerY);
	public:
		Renderer(android_app *const appData):
			appData(appData),
			display(EGL_NO_DISPLAY), surface(EGL_NO_SURFACE), context(EGL_NO_CONTEXT),
			width(0), height(0)
		{
			initRenderer();
		}
		virtual ~Renderer();
		void handleInput();
		void render();

		oboe::DataCallbackResult onAudioReady(
			oboe::AudioStream *currentAudioStream, void *audioBuffer, std::int32_t frames
		) override;
};

#endif // YUBINOBUTAI_RENDERER_H