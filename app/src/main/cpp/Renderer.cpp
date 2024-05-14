#include "Renderer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <sstream>
#include <vector>

#include <GLES3/gl3.h>
#include <android/imagedecoder.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <oboe/Oboe.h>

#include <audio/AggregateAudioStream.h>
#include <audio/PreloadedAudioTrack.h>
#include <audio/StreamingAudioStream.h>
#include "AndroidOut.h"
#include "BasicData.h"
#include "Shader.h"
#include "TextureAsset.h"
#include "Utility.h"

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) do { aout << #s": "<< glGetString(s) << std::endl; } while (false)

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 *
 * This works by creating an istringstream of the input c-style string. Then that is used to create
 * a vector -- each element of the vector is a new element in the input string. Finally a foreach
 * loop consumes this and outputs it to logcat using @a aout
 */
#define PRINT_GL_STRING_AS_LIST(s) do {\
	std::istringstream extensionStream(reinterpret_cast<const char*>(glGetString(s)));\
	const std::vector<std::string> extensionList(\
		std::istream_iterator<std::string>{extensionStream},\
		std::istream_iterator<std::string>()\
	);\
	aout << #s":\n";\
	for (const auto &extension : extensionList) aout << extension << "\n";\
	aout << std::endl;\
} while (false)

void Renderer::initRenderer() {
	const auto assetManager = appData->activity->assetManager;

	constexpr EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};

	auto initialDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(initialDisplay, nullptr, nullptr);

	EGLint configCount;
	eglChooseConfig(initialDisplay, attribs, nullptr, 0, &configCount);
	std::vector<EGLConfig> supportedConfigs(configCount);
	eglChooseConfig(initialDisplay, attribs, supportedConfigs.data(), configCount, &configCount);
	auto config = *std::find_if(
		supportedConfigs.begin(), supportedConfigs.end(),
		[&initialDisplay](const EGLConfig &config) {
			EGLint red, green, blue, depth;
			if (
				eglGetConfigAttrib(initialDisplay, config, EGL_RED_SIZE, &red)
				&& eglGetConfigAttrib(initialDisplay, config, EGL_GREEN_SIZE, &green)
				&& eglGetConfigAttrib(initialDisplay, config, EGL_BLUE_SIZE, &blue)
				&& eglGetConfigAttrib(initialDisplay, config, EGL_DEPTH_SIZE, &depth)
			) {
				aout << "Found config with " << red << ", " << green << ", " << blue << ", "
					 << depth << std::endl;
				return red == 8 && green == 8 && blue == 8 && depth == 24;
			}
			return false;
		}
	);
	aout << "Found " << configCount << " configs" << std::endl;
	aout << "Chose " << config << std::endl;

	EGLint format;
	eglGetConfigAttrib(initialDisplay, config, EGL_NATIVE_VISUAL_ID, &format);
	EGLSurface initialSurface = eglCreateWindowSurface(initialDisplay, config, appData->window, nullptr);
	EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	EGLContext initialContext = eglCreateContext(initialDisplay, config, nullptr, contextAttributes);
	assert(eglMakeCurrent(initialDisplay, initialSurface, initialSurface, initialContext));

	display = initialDisplay;
	surface = initialSurface;
	context = initialContext;

	// Make width and height invalid so it gets updated the first frame in `updateRenderArea`.
	width = -1;
	height = -1;

	PRINT_GL_STRING(GL_VENDOR);
	PRINT_GL_STRING(GL_RENDERER);
	PRINT_GL_STRING(GL_VERSION);
	PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);

	// setup any other gl related global states
	glClearColor(0.f, 0.f, 0.f, 1.f);

	// enable alpha globally for now, you probably don't want to do this in a game
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);

	font.emplace(assetManager, "SegoeUi");
	testLine.emplace();

	aggregateStream.reset(new AggregateAudioStream());
	musicStream.reset(new StreamingAudioStream(assetManager, "Can't let go 2 (GD cut).mp3"));
	effectTrack.reset(new PreloadedAudioTrack(assetManager, "Hit.wav"));
	aggregateStream->play(musicStream.get());

	oboe::AudioStreamBuilder audioStreamBuilder;
	audioStreamBuilder.setDirection(oboe::Direction::Output);
	audioStreamBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
	audioStreamBuilder.setSharingMode(oboe::SharingMode::Exclusive);
	audioStreamBuilder.setFormat(oboe::AudioFormat::Float);
	audioStreamBuilder.setFormatConversionAllowed(true);
	audioStreamBuilder.setSampleRate(48000);
	audioStreamBuilder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);
	audioStreamBuilder.setChannelCount(oboe::ChannelCount::Stereo);
	audioStreamBuilder.setDataCallback(this);
	audioStreamBuilder.openStream(audioStream);
	audioStream->requestStart();

	const auto chartAsset = AAssetManager_open(assetManager, "chart.txt", AASSET_MODE_BUFFER);
	const char *const chartBuffer = static_cast<const char*>(AAsset_getBuffer(chartAsset));
	std::istringstream chartStream(std::string(chartBuffer, AAsset_getLength(chartAsset)));
	while (true) {
		Note note;
		chartStream >> note.time;
		if (!chartStream) break;
		chartStream >> note.position;
		notes.push_back(note);
	}
	nextNote = notes.begin();
	AAsset_close(chartAsset);
}

void Renderer::updateRenderArea() {
	EGLint newWidth, newHeight;
	eglQuerySurface(display, surface, EGL_WIDTH, &newWidth);
	eglQuerySurface(display, surface, EGL_HEIGHT, &newHeight);

	if (newWidth != width || newHeight != height) {
		width = newWidth;
		height = newHeight;
		glViewport(0, 0, width, height);
	}
}

void Renderer::handleInput() {
	// handle all queued inputs
	auto *inputBuffer = android_app_swap_input_buffers(appData);
	if (!inputBuffer) {
		// no inputs yet.
		return;
	}

	// handle motion events (motionEventsCounts can be 0).
	for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
		auto &motionEvent = inputBuffer->motionEvents[i];
		auto action = motionEvent.action;

		// Find the pointer index, mask and bitshift to turn it into a readable value.
		auto pointerIndex
			= (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

		// get the x and y position of this event if it is not ACTION_MOVE.
		auto &pointer = motionEvent.pointers[pointerIndex];
		const float
			x = GameActivityPointerAxes_getX(&pointer),
			y = GameActivityPointerAxes_getY(&pointer);

		// determine the action type and process the event accordingly.
		switch (action & AMOTION_EVENT_ACTION_MASK) {
			case AMOTION_EVENT_ACTION_DOWN:
			case AMOTION_EVENT_ACTION_POINTER_DOWN: {
				onTap(x, y);
				//aout << "(" << pointer.id << ", " << x << ", " << y << ") Pointer down";
				break;
			}
			case AMOTION_EVENT_ACTION_CANCEL:
				// treat the CANCEL as an UP event: doing nothing in the appData, except
				// removing the pointer from the cache if pointers are locally saved.
				// code pass through on purpose.
			case AMOTION_EVENT_ACTION_UP:
			case AMOTION_EVENT_ACTION_POINTER_UP:
				//aout << "(" << pointer.id << ", " << x << ", " << y << ") Pointer up";
				break;
			case AMOTION_EVENT_ACTION_MOVE:
				// There is no pointer index for ACTION_MOVE, only a snapshot of
				// all active pointers; appData needs to cache previous active pointers
				// to figure out which ones are actually moved.
				/*
				for (auto index = 0; index < motionEvent.pointerCount; index++) {
					pointer = motionEvent.pointers[index];
					x = GameActivityPointerAxes_getX(&pointer);
					y = GameActivityPointerAxes_getY(&pointer);
					aout << "(" << pointer.id << ", " << x << ", " << y << ")";

					if (index != (motionEvent.pointerCount - 1)) aout << ",";
					aout << " ";
				}
				aout << "Pointer move";
				*/
				break;
			default:
				aout << "Unknown MotionEvent action: " << action;
		}
		//aout << std::endl;
	}
	// clear the motion input count in this buffer for main thread to re-use.
	android_app_clear_motion_events(inputBuffer);

	// handle input key events.
	for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
		auto &keyEvent = inputBuffer->keyEvents[i];
		aout << "Key: " << keyEvent.keyCode <<" ";
		switch (keyEvent.action) {
			case AKEY_EVENT_ACTION_DOWN:
				aout << "Key down";
				break;
			case AKEY_EVENT_ACTION_UP:
				aout << "Key up";
				break;
			case AKEY_EVENT_ACTION_MULTIPLE:
				// Deprecated since Android API level 29.
				aout << "Multiple key actions";
				break;
			default:
				aout << "Unknown KeyEvent action: " << keyEvent.action;
		}
		aout << std::endl;
	}
	// clear the key input count too.
	android_app_clear_key_events(inputBuffer);
}

void Renderer::onTap(const float pointerX, const float pointerY) {
	const double
		factor = glm::tan(glm::radians(35.)) * 7.,
		worldX = (static_cast<double>(pointerX) - width / 2.) / height * factor,
		worldY = - static_cast<double>(pointerY) / height * factor + 4.;
	if (glm::abs(worldX) > 3 || glm::abs(worldY) > 1) return;

	auto effect = std::make_unique<PreloadedAudioStream>(*effectTrack);
	const auto handle = aggregateStream->play(effect.get());
	playingEffects.push_back({std::move(effect), handle});

	const int column = static_cast<int>((worldX + 3.) * 2.);
	for (auto iterator = nextNote; iterator != notes.end(); ++iterator) {
		auto &note = *iterator;
		if (note.time - time > 100.) break;
		if (!note.hit && time - note.time < 100. && column >= note.position && column < note.position + 3) {
			note.hit = true;
			++hitCount;
			return;
		}
	}
}

void Renderer::render() {
	// Check to see if the surface has changed size. This is necessary to do every frame when
	// using immersive mode as you'll get no other notification that your renderable area has
	// changed.
	updateRenderArea();

	playingEffects.resize(std::remove_if(
		playingEffects.begin(), playingEffects.end(),
		[this](const auto &effect) { return !aggregateStream->isPlaying(effect.handle); }
	) - playingEffects.begin());

	glClear(GL_COLOR_BUFFER_BIT);

	const auto camera = glm::translate(
		glm::translate(glm::identity<glm::mat4>(), glm::vec3(0.f, 1.f, 0.f))
			* glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.f, 2.f, 1.f))
			* glm::perspectiveFov<float>(glm::radians(70.f), width, height * 2.f, 0.01f, 1000.f),
		glm::vec3(0.f, -4.f, -7.f)
	);
	for (int i = -2; i != 3; ++i) testLine->render(
		glm::translate(camera, glm::vec3(i, 0.f, 7.f)),
		0.005f, 1000.f, {1.f, 1.f, 1.f, 0.7f}
	);
	for (int i = -3; i != 3; ++i) testLine->render(
		glm::translate(camera, glm::vec3(i + 0.5f, 0.f, 0.f)),
		0.005f, 0.5f, {1.f, 1.f, 1.f, 0.7f}
	);
	testLine->render(camera, 6.f, 0.01f, {1.f, 1.f, 1.f, 1.f});
	testLine->render(glm::translate(camera, glm::vec3(0.f, 0.f, -0.5f)), 6.f, 0.01f, {1.f, 1.f, 1.f, 1.f});
	testLine->render(glm::translate(camera, glm::vec3(-3.f, 0.f, 7.f)), 0.01f, 1000.f, {1.f, 1.f, 1.f, 1.f});
	testLine->render(glm::translate(camera, glm::vec3(3.f, 0.f, 7.f)), 0.01f, 1000.f, {1.f, 1.f, 1.f, 1.f});

	time = musicStream->getTime();
	const int minVisibleTime = static_cast<int>(time) - 200;
	while (nextNote != notes.end() && nextNote->time < minVisibleTime) ++nextNote;
	const int maxVisibleTime = static_cast<int>(time) + 10000;
	for (
		auto currentNote = nextNote;
		currentNote != notes.end() && currentNote->time < maxVisibleTime;
		++currentNote
	) {
		const auto &note = *currentNote;
		if (!note.hit) testLine->render(
			glm::translate(camera, glm::vec3(note.position / 2.f - 2.25f, 0.f, (time - note.time) / 1000. * 15.)),
			1.5f, 0.5f, {1.f, 1.f, 0.f, 1.f}
		);
	}

	font->render(
		"Hit: " + std::to_string(hitCount) + " / " + std::to_string(nextNote - notes.begin()), 64.f,
		glm::translate(glm::ortho<float>(0, width, 0, height), glm::vec3(64.f, height - 128.f, 0.f)),
		{0.44f, 0.69f, 1.f, 1.f}
	);

	assert(eglSwapBuffers(display, surface) == EGL_TRUE);
}

oboe::DataCallbackResult Renderer::onAudioReady(
	oboe::AudioStream *const currentAudioStream, void *const audioBuffer, const std::int32_t frames
) {
	float *originalBuffer = static_cast<float*>(audioBuffer);
	float *buffer = originalBuffer;
	int actualFrames = aggregateStream->getAudio(buffer, frames);
	if (buffer != originalBuffer) std::copy(buffer, buffer + frames * 2, originalBuffer);
	return actualFrames == frames ? oboe::DataCallbackResult::Continue : oboe::DataCallbackResult::Stop;
}

Renderer::~Renderer() {
	audioStream->close();

	if (display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (context != EGL_NO_CONTEXT) {
			eglDestroyContext(display, context);
			context = EGL_NO_CONTEXT;
		}
		if (surface != EGL_NO_SURFACE) {
			eglDestroySurface(display, surface);
			surface = EGL_NO_SURFACE;
		}
		eglTerminate(display);
		display = EGL_NO_DISPLAY;
	}
}