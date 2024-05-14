#include <jni.h>

//#include <game-activity/GameActivity.cpp>
//#include <game-text-input/gametextinput.cpp>

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "AndroidOut.h"
#include "Renderer.h"

extern "C" {

//#include <game-activity/native_app_glue/android_native_app_glue.c>

/*!
 * Handles commands sent to this Android application
 * @param appData the app the commands are coming from
 * @param cmd the command to handle
 */
void handleCommand(android_app *const appData, const int32_t command) {
	switch (command) {
		case APP_CMD_INIT_WINDOW:
			// A new window is created, associate a renderer with it. You may replace this with a
			// "game" class if that suits your needs. Remember to change all instances of userData
			// if you change the class here as a reinterpret_cast is dangerous this in the
			// android_main function and the APP_CMD_TERM_WINDOW handler case.
			appData->userData = new Renderer(appData);
			break;
		case APP_CMD_TERM_WINDOW:
			// The window is being destroyed. Use this to clean up your userData to avoid leaking
			// resources.
			//
			// We have to check if userData is assigned just in case this comes in really quickly
			if (appData->userData) {
				auto *renderer = reinterpret_cast<Renderer*>(appData->userData);
				appData->userData = nullptr;
				delete renderer;
			}
			break;
	}
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool filterEvent(const GameActivityMotionEvent *const motionEvent) {
	auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
	return sourceClass == AINPUT_SOURCE_CLASS_POINTER
		|| sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK;
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *const appData) {
	// Can be removed, useful to ensure your code is running
	aout << "Starting game." << std::endl;

	// Register an event handler for Android events
	appData->onAppCmd = handleCommand;

	// Set input event filters (set it to NULL if the app wants to process all inputs).
	// Note that for key inputs, this example uses the default default_key_filter()
	// implemented in android_native_app_glue.c.
	android_app_set_motion_event_filter(appData, filterEvent);

	// This sets up a typical game/event loop. It will run until the app is destroyed.
	int events;
	android_poll_source *pollSource;
	do {
		if (ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&pollSource)) >= 0) {
			if (pollSource) pollSource->process(appData, pollSource);
		}

		if (appData->userData) {
			auto &renderer = *reinterpret_cast<Renderer*>(appData->userData);
			renderer.handleInput();
			renderer.render();
		}
	} while (!appData->destroyRequested);
}

} // extern "C"