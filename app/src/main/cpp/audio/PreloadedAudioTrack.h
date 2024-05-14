#ifndef YUBINOBUTAI_PRELOADEDAUDIOTRACK_H
#define YUBINOBUTAI_PRELOADEDAUDIOTRACK_H

#include <string>
#include <vector>

#include <android/asset_manager.h>

class PreloadedAudioTrack final {
	private:
		std::vector<float> audioData;
		int length;
	public:
		PreloadedAudioTrack(AAssetManager *assetManager, const std::string &name);
		int getLength() const {
			return length;
		}
		const std::vector<float>& getAudioData() const {
			return audioData;
		}
};

#endif // YUBINOBUTAI_PRELOADEDAUDIOTRACK_H