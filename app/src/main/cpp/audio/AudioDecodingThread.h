#ifndef YUBINOBUTAI_AUDIODECODINGTHREAD_H
#define YUBINOBUTAI_AUDIODECODINGTHREAD_H

#include <atomic>
#include <thread>

#include <ConcurrentQueue/blockingconcurrentqueue.h>

#include "StreamingAudioStream.h"

// For use with `StreamingAudioStream`.
class AudioDecodingThread final {
	public:
		struct Task {
			StreamingAudioStream::Internal *stream;
			bool isFinalization; // If `true`, this stream is to be destroyed rather than decoded into.
		};
	private:
		std::thread thread;
		moodycamel::BlockingConcurrentQueue<Task> tasks;

		void run() {
			Task task;
			tasks.wait_dequeue(task);
			while (true) {
				if (task.stream == nullptr) break;
				if (task.isFinalization) delete task.stream;
				else task.stream->fill();
				tasks.wait_dequeue(task);
			}
		}
	public:
		AudioDecodingThread(): thread([this] { run(); }) {}
		// Must only be destroyed after having destroyed all `StreamingAudioStream`s associated with this thread.
		~AudioDecodingThread() {
			tasks.enqueue({nullptr, true});
			thread.join();
		}
		void addTask(Task task) {
			tasks.enqueue(task);
		}
};

#endif // YUBINOBUTAI_AUDIODECODINGTHREAD_H