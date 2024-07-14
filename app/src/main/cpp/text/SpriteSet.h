#ifndef TEXT_SPRITESET_H
#define TEXT_SPRITESET_H

#include <cstdlib>
#include <optional>
#include <vector>

#include <GLES3/gl3.h>

#include "LruList.h"

class SpriteSet final {
	private:
		using LruData = unsigned;
		struct Page {
			unsigned previousIndex, nextIndex;
			unsigned firstShelfIndex;
			unsigned textureId;
			unsigned firstDirtyY, pastLastDirtyY;
			std::optional<std::vector<unsigned char>> textureData;
		};
		struct Shelf {
			unsigned y, height;
			unsigned previousIndex, nextIndex;
			unsigned pageIndex;
			unsigned firstSlotIndex;
			bool allocated;
		};
		struct Slot {
			unsigned x, width, height;
			unsigned previousIndex, nextIndex;
			unsigned shelfIndex;
			unsigned epoch;
			bool allocated;
			LruList<LruData>::Handle lruHandle;
		};

		int spritePadding;
		std::vector<Page> pagePool{1 << 3};
		std::vector<Shelf> shelfPool{1 << 8};
		std::vector<Slot> slotPool{1 << 10};
		unsigned
			firstPageIndex = -1, lastPageIndex = 0,
			nextFreeShelfIndex = 0, nextFreeSlotIndex = 0;
		unsigned firstPageAllocatedPixels = 0;
		unsigned currentEpoch = 0;
		LruList<LruData> lruList;

		unsigned allocate(unsigned width, unsigned height);
		unsigned tryAllocateInPage(unsigned pageIndex, unsigned width, unsigned height);
		unsigned remove(unsigned slotIndex);
	public:
		struct Handle {
			unsigned slotIndex;
			unsigned epoch;
		};
		struct SpriteData {
			unsigned textureId, x, y, width, height;
		};

		SpriteSet(int spritePadding);
		~SpriteSet();
		void tick();
		Handle add(unsigned width, unsigned height, const unsigned char *data);
		bool ping(Handle handle);
		bool isAlive(Handle handle) const;
		SpriteData get(Handle handle) const;
		void syncToGpu();
		void dump(std::vector<SpriteData> &sprites) const;
};

#endif // TEXT_SPRITESET_H