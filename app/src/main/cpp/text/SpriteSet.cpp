#include <algorithm>

#include "SpriteSet.h"

namespace {
	constexpr unsigned nullIndex = -1;
	constexpr unsigned pageSize = 1024;
	constexpr unsigned evictionThreshold = pageSize * pageSize * 3 / 4;
	constexpr unsigned evictionTime = 600;
	constexpr unsigned maxChangedPixels = 256 * 256;
	constexpr unsigned splitThreshold = 8;

	template<typename T>
	void initializePool(std::vector<T> &pool) {
		const unsigned
			poolSize = static_cast<unsigned>(pool.size()),
			lastPoolIndex = poolSize - 1;
		for (unsigned i = 0; i != lastPoolIndex; ++i) pool[i].nextIndex = i + 1;
		pool[lastPoolIndex].nextIndex = nullIndex;
	}

	template<typename T>
	unsigned allocateEntry(std::vector<T> &pool, unsigned &nextFreeIndex) {
		if (nextFreeIndex == nullIndex) {
			const unsigned
				oldPoolSize = static_cast<unsigned>(pool.size()),
				newPoolSize = oldPoolSize << 1,
				lastPoolIndex = newPoolSize - 1;
			pool.resize(newPoolSize);
			for (unsigned i = oldPoolSize; i != lastPoolIndex; ++i) pool[i].nextIndex = i + 1;
			pool[lastPoolIndex].nextIndex = nullIndex;
			nextFreeIndex = oldPoolSize;
		}
		const unsigned index = nextFreeIndex;
		nextFreeIndex = pool[index].nextIndex;
		return index;
	}

	template<typename T>
	void freeEntry(std::vector<T> &pool, unsigned &nextFreeIndex, const unsigned index) {
		pool[index].nextIndex = nextFreeIndex;
		nextFreeIndex = index;
	}
}

SpriteSet::SpriteSet(const int spritePadding): spritePadding(spritePadding) {
	initializePool(pagePool);
	initializePool(shelfPool);
	initializePool(slotPool);
}

void SpriteSet::tick() {
	++currentEpoch;
	lruList.tick();
	if (
		firstPageIndex == nullIndex
		|| (firstPageIndex == lastPageIndex && firstPageAllocatedPixels < evictionThreshold)
	) return;
	unsigned changedPixels = 0;
	while (changedPixels <= maxChangedPixels && lruList.getLastEntryAge() > evictionTime) {
		changedPixels += remove(*lruList.getLast());
		lruList.evictLast();
	}
	// Try compaction by moving sprites from the last page to the first page.
	while (changedPixels <= maxChangedPixels && firstPageIndex != lastPageIndex) {
		const Page &sourcePage = pagePool[lastPageIndex];
		unsigned sourceShelfIndex = sourcePage.firstShelfIndex;
		while (!shelfPool[sourceShelfIndex].allocated) sourceShelfIndex = shelfPool[sourceShelfIndex].nextIndex;
		const Shelf &sourceShelf = shelfPool[sourceShelfIndex];
		unsigned sourceSlotIndex = sourceShelf.firstSlotIndex;
		while (!slotPool[sourceSlotIndex].allocated) sourceSlotIndex = slotPool[sourceSlotIndex].nextIndex;
		const Slot &sourceSlot = slotPool[sourceSlotIndex];
		const unsigned destinationSlotIndex = tryAllocateInPage(firstPageIndex, sourceSlot.width, sourceSlot.height);
		if (destinationSlotIndex == nullIndex) break;
		const Slot &destinationSlot = slotPool[destinationSlotIndex];
		const Shelf &destinationShelf = shelfPool[destinationSlot.shelfIndex];
		Page &destinationPage = pagePool[firstPageIndex];
		const unsigned
			sourceX = sourceSlot.x, sourceY = sourceShelf.y,
			destinationX = destinationSlot.x, destinationY = destinationShelf.y,
			width = sourceSlot.width, height = sourceSlot.height;
		for (unsigned localY = 0; localY != height; ++localY) {
			const auto dataStart = sourcePage.textureData->begin() + ((sourceY + localY) * pageSize + sourceX);
			std::copy(
				dataStart, dataStart + width,
				destinationPage.textureData->begin() + ((destinationY + localY) * pageSize + destinationX)
			);
		}
		destinationPage.firstDirtyY = std::min(destinationPage.firstDirtyY, destinationY);
		destinationPage.pastLastDirtyY = std::max(destinationPage.pastLastDirtyY, destinationY + height);
		changedPixels += remove(sourceSlotIndex);
	}
}

SpriteSet::Handle SpriteSet::add(const unsigned width, const unsigned height, const unsigned char *const data) {
	const unsigned paddedWidth = width + spritePadding * 2, paddedHeight = height + spritePadding * 2;
	const unsigned slotIndex = allocate(paddedWidth, paddedHeight);
	const Slot &slot = slotPool[slotIndex];
	const Shelf &shelf = shelfPool[slot.shelfIndex];
	Page &page = pagePool[shelf.pageIndex];
	for (
		unsigned i = 0, topPaddingY = shelf.y, bottomPaddingY = shelf.y + paddedHeight - 1;
		i != spritePadding; ++i, ++topPaddingY, --bottomPaddingY
	) {
		auto textureStart = page.textureData->begin() + (topPaddingY * pageSize + slot.x);
		std::fill(textureStart, textureStart + paddedWidth, 0);
		textureStart = page.textureData->begin() + (bottomPaddingY * pageSize + slot.x);
		std::fill(textureStart, textureStart + paddedWidth, 0);
	}
	const unsigned textureY = shelf.y + spritePadding;
	for (unsigned localY = 0; localY != height; ++localY) {
		const unsigned char *const dataStart = data + (localY * width);
		const auto textureStart = page.textureData->begin() + ((textureY + localY) * pageSize + slot.x);
		std::fill(textureStart, textureStart + spritePadding, 0);
		std::fill(textureStart + spritePadding + width, textureStart + paddedWidth, 0);
		std::copy(dataStart, dataStart + width, textureStart + spritePadding);
	}
	page.firstDirtyY = std::min(page.firstDirtyY, shelf.y);
	page.pastLastDirtyY = std::max(page.pastLastDirtyY, shelf.y + paddedHeight);
	return {slotIndex, slot.epoch};
}

unsigned SpriteSet::allocate(const unsigned width, const unsigned height) {
	// Try to allocate in an existing page.
	if (firstPageIndex != nullIndex) {
		unsigned pageIndex = firstPageIndex;
		while (true) {
			const unsigned slotIndex = tryAllocateInPage(pageIndex, width, height);
			if (slotIndex != nullIndex) return slotIndex;
			if (pageIndex == lastPageIndex) break;
			pageIndex = pagePool[pageIndex].nextIndex;
		}
	}

	// No page could fit, allocate a new page.
	if (firstPageIndex == nullIndex) {
		firstPageIndex = lastPageIndex;
		Page &page = pagePool[lastPageIndex];
		page.previousIndex = nullIndex;
	} else {
		unsigned pageIndex = pagePool[lastPageIndex].nextIndex;
		if (pageIndex == nullIndex) {
			const unsigned
				oldPoolSize = static_cast<unsigned>(pagePool.size()),
				newPoolSize = oldPoolSize << 1,
				lastPoolIndex = newPoolSize - 1;
			pagePool.resize(newPoolSize);
			for (unsigned i = oldPoolSize; i != lastPoolIndex; ++i) pagePool[i].nextIndex = i + 1;
			pagePool[lastPoolIndex].nextIndex = nullIndex;
			pagePool[lastPageIndex].nextIndex = oldPoolSize;
			pageIndex = oldPoolSize;
		}
		pagePool[pageIndex].previousIndex = lastPageIndex;
		lastPageIndex = pageIndex;
	}

	Page &page = pagePool[lastPageIndex];
	page.textureData.emplace(pageSize * pageSize);
	page.firstDirtyY = pageSize;
	page.pastLastDirtyY = 0;
	glGenTextures(1, &page.textureId);
	glBindTexture(GL_TEXTURE_2D, page.textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, pageSize, pageSize, 0, GL_RED, GL_UNSIGNED_BYTE, page.textureData->data());

	const unsigned shelfIndex = allocateEntry<Shelf>(shelfPool, nextFreeShelfIndex);
	const unsigned slotIndex = allocateEntry<Slot>(slotPool, nextFreeSlotIndex);
	slotPool[slotIndex] = {0, pageSize, 0, nullIndex, nullIndex, shelfIndex, false};
	shelfPool[shelfIndex] = {0, pageSize, nullIndex, nullIndex, lastPageIndex, slotIndex, false};
	page.firstShelfIndex = shelfIndex;
	return tryAllocateInPage(lastPageIndex, width, height);
}

unsigned SpriteSet::tryAllocateInPage(const unsigned pageIndex, const unsigned width, const unsigned height) {
	Page &page = pagePool[pageIndex];
	unsigned selectedShelfIndex = nullIndex, selectedSlotIndex = nullIndex, selectedShelfHeight = -1;
	for (
		unsigned shelfIndex = page.firstShelfIndex;
		shelfIndex != nullIndex; shelfIndex = shelfPool[shelfIndex].nextIndex
	) {
		const Shelf &shelf = shelfPool[shelfIndex];
		if (
			shelf.height < height || shelf.height >= selectedShelfHeight
			|| (shelf.allocated && shelf.height > height * 3 / 2)
		) continue;
		bool found = false;
		for (
			unsigned slotIndex = shelf.firstSlotIndex;
			slotIndex != nullIndex; slotIndex = slotPool[slotIndex].nextIndex
		) {
			const Slot &slot = slotPool[slotIndex];
			if (!slot.allocated && slot.width >= width) {
				selectedShelfIndex = shelfIndex;
				selectedSlotIndex = slotIndex;
				selectedShelfHeight = shelf.height;
				found = true;
				break;
			}
		}
		if (found && shelf.height == height) break;
	}
	if (selectedSlotIndex == nullIndex) return nullIndex;
	Shelf &shelf = shelfPool[selectedShelfIndex];
	if (!shelf.allocated) {
		shelf.allocated = true;
		if (shelf.height - height >= splitThreshold) {
			unsigned newShelfIndex = allocateEntry<Shelf>(shelfPool, nextFreeShelfIndex);
			unsigned newSlotIndex = allocateEntry<Slot>(slotPool, nextFreeSlotIndex);
			slotPool[newSlotIndex] = {0, pageSize, 0, nullIndex, nullIndex, newShelfIndex, false};
			shelfPool[newShelfIndex] = {
				shelf.y + height, shelf.height - height,
				selectedShelfIndex, shelf.nextIndex, pageIndex, newSlotIndex, false
			};
			if (shelf.nextIndex != nullIndex) shelfPool[shelf.nextIndex].previousIndex = newShelfIndex;
			shelf.nextIndex = newShelfIndex;
			shelf.height = height;
		}
	}
	Slot &slot = slotPool[selectedSlotIndex];
	if (slot.width - width >= splitThreshold) {
		unsigned newSlotIndex = allocateEntry<Slot>(slotPool, nextFreeSlotIndex);
		slotPool[newSlotIndex] = {
			slot.x + width, slot.width - width, 0, selectedSlotIndex, slot.nextIndex, selectedShelfIndex, false
		};
		if (slot.nextIndex != nullIndex) slotPool[slot.nextIndex].previousIndex = newSlotIndex;
		slot.nextIndex = newSlotIndex;
		slot.width = width;
	}
	slot.allocated = true;
	slot.height = height;
	slot.lruHandle = lruList.add(selectedSlotIndex);
	if (pageIndex == firstPageIndex) firstPageAllocatedPixels += width * shelf.height;
	return selectedSlotIndex;
}

unsigned SpriteSet::remove(const unsigned slotIndex) {
	Slot &slot = slotPool[slotIndex];
	Shelf &shelf = shelfPool[slot.shelfIndex];
	const unsigned pageIndex = shelf.pageIndex;
	Page &page = pagePool[pageIndex];
	const unsigned slotPixels = slot.width * slot.height;
	slot.allocated = false;
	++slot.epoch;

	// Merge consecutive empty slots.
	if (slot.nextIndex != nullIndex) {
		Slot &nextSlot = slotPool[slot.nextIndex];
		if (!nextSlot.allocated) {
			slot.width += nextSlot.width;
			const unsigned nextIndex = slot.nextIndex;
			slot.nextIndex = nextSlot.nextIndex;
			freeEntry<Slot>(slotPool, nextFreeSlotIndex, nextIndex);
			if (slot.nextIndex != nullIndex) slotPool[slot.nextIndex].previousIndex = slotIndex;
		}
	}
	if (slot.previousIndex != nullIndex) {
		Slot &previousSlot = slotPool[slot.previousIndex];
		if (!previousSlot.allocated) {
			slot.x -= previousSlot.width;
			slot.width += previousSlot.width;
			const unsigned previousIndex = slot.previousIndex;
			slot.previousIndex = previousSlot.previousIndex;
			freeEntry<Slot>(slotPool, nextFreeSlotIndex, previousIndex);
			if (slot.previousIndex == nullIndex) {
				shelf.firstSlotIndex = slotIndex;
				if (slot.nextIndex == nullIndex) shelf.allocated = false;
			} else {
				slotPool[slot.previousIndex].nextIndex = slotIndex;
			}
		}
	}

	// Merge consecutive empty shelves.
	if (shelf.allocated) return slotPixels;
	if (shelf.nextIndex != nullIndex) {
		Shelf &nextShelf = shelfPool[shelf.nextIndex];
		if (!nextShelf.allocated) {
			shelf.height += nextShelf.height;
			const unsigned nextIndex = shelf.nextIndex;
			shelf.nextIndex = nextShelf.nextIndex;
			freeEntry<Shelf>(shelfPool, nextFreeShelfIndex, nextIndex);
			if (shelf.nextIndex != nullIndex) shelfPool[shelf.nextIndex].previousIndex = slot.shelfIndex;
		}
	}
	if (shelf.previousIndex != nullIndex) {
		Shelf &previousShelf = shelfPool[shelf.previousIndex];
		if (!previousShelf.allocated) {
			shelf.y -= previousShelf.height;
			shelf.height += previousShelf.height;
			const unsigned previousIndex = shelf.previousIndex;
			shelf.previousIndex = previousShelf.previousIndex;
			freeEntry<Slot>(slotPool, nextFreeSlotIndex, previousShelf.firstSlotIndex);
			freeEntry<Shelf>(shelfPool, nextFreeShelfIndex, previousIndex);
			if (shelf.previousIndex == nullIndex) page.firstShelfIndex = slot.shelfIndex;
			else shelfPool[shelf.previousIndex].nextIndex = slot.shelfIndex;
		}
	}

	// Deallocate the page if it becomes empty, except when it's the first one.
	if (pageIndex == firstPageIndex) {
		firstPageAllocatedPixels -= slot.width * shelf.height;
		return slotPixels;
	}
	if (shelf.height != pageSize) return slotPixels;
	glDeleteTextures(1, &page.textureId);
	page.textureData.reset();
	pagePool[page.previousIndex].nextIndex = page.nextIndex;
	if (pageIndex != lastPageIndex) pagePool[page.nextIndex].previousIndex = page.previousIndex;
	pagePool[lastPageIndex].nextIndex = pageIndex;
	return slotPixels;
}

bool SpriteSet::ping(const Handle handle) {
	Slot &slot = slotPool[handle.slotIndex];
	if (slot.epoch != handle.epoch) return false;
	lruList.ping(slot.lruHandle);
	return true;
}

bool SpriteSet::isAlive(const Handle handle) const {
	return slotPool[handle.slotIndex].epoch == handle.epoch;
}

SpriteSet::SpriteData SpriteSet::get(const Handle handle) const {
	const Slot &slot = slotPool[handle.slotIndex];
	return {
		pagePool[shelfPool[slot.shelfIndex].pageIndex].textureId,
		slot.x + spritePadding, shelfPool[slot.shelfIndex].y + spritePadding,
		slot.width - spritePadding * 2, slot.height - spritePadding * 2
	};
}

void SpriteSet::syncToGpu() {
	if (firstPageIndex == nullIndex) return;
	unsigned pageIndex = firstPageIndex;
	while (true) {
		Page &page = pagePool[pageIndex];
		if (page.firstDirtyY < page.pastLastDirtyY) {
			glBindTexture(GL_TEXTURE_2D, page.textureId);
			glTexSubImage2D(
				GL_TEXTURE_2D, 0, 0, page.firstDirtyY, pageSize, page.pastLastDirtyY - page.firstDirtyY,
				GL_RED, GL_UNSIGNED_BYTE, page.textureData->data() + page.firstDirtyY * pageSize
			);
			page.firstDirtyY = pageSize;
			page.pastLastDirtyY = 0;
		}
		if (pageIndex == lastPageIndex) break;
		pageIndex = pagePool[pageIndex].nextIndex;
	}
}

void SpriteSet::dump(std::vector<SpriteSet::SpriteData> &sprites) const {
	if (firstPageIndex == nullIndex) return;
	for (
		unsigned shelfIndex = pagePool[firstPageIndex].firstShelfIndex;
		shelfIndex != nullIndex; shelfIndex = shelfPool[shelfIndex].nextIndex
	) {
		const unsigned shelfY = shelfPool[shelfIndex].y;
		for (
			unsigned slotIndex = shelfPool[shelfIndex].firstSlotIndex;
			slotIndex != nullIndex; slotIndex = slotPool[slotIndex].nextIndex
		) {
			const Slot &slot = slotPool[slotIndex];
			if (slot.allocated) sprites.push_back({slot.x, shelfY, slot.width, slot.height});
		}
	}
}

SpriteSet::~SpriteSet() {
	if (firstPageIndex != nullIndex) {
		unsigned pageIndex = firstPageIndex;
		while (true) {
			glDeleteTextures(1, &pagePool[pageIndex].textureId);
			if (pageIndex == lastPageIndex) break;
			pageIndex = pagePool[pageIndex].nextIndex;
		}
	}
}