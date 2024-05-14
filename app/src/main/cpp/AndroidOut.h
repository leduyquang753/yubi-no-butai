#ifndef YUBINOBUTAI_ANDROIDOUT_H
#define YUBINOBUTAI_ANDROIDOUT_H

#include <android/log.h>
#include <sstream>

extern std::ostream aout;

class AndroidOut: public std::stringbuf {
	private:
		const char* logTag_;
	protected:
		virtual int sync() override {
			__android_log_print(ANDROID_LOG_DEBUG, logTag_, "%s", str().c_str());
			str("");
			return 0;
		}
	public:
		AndroidOut(const char* kLogTag): logTag_(kLogTag) {}
};

#endif // YUBINOBUTAI_ANDROIDOUT_H