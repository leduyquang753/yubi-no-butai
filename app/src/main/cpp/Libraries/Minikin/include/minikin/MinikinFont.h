/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file was modified by Lê Duy Quang.
 */

#ifndef MINIKIN_MINIKIN_FONT_H
#define MINIKIN_MINIKIN_FONT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "minikin/FontVariation.h"

namespace minikin {

class FontFakery;
struct MinikinExtent;
struct MinikinPaint;
struct MinikinRect;

// An abstraction for platform fonts, allowing Minikin to be used with
// multiple actual implementations of fonts.
class MinikinFont {
public:
    MinikinFont() {}

    virtual ~MinikinFont() {}

    virtual float GetHorizontalAdvance(uint32_t glyph_id, const MinikinPaint& paint,
                                       const FontFakery& fakery) const = 0;
    virtual void GetHorizontalAdvances(uint16_t* glyph_ids, uint32_t count,
                                       const MinikinPaint& paint, const FontFakery& fakery,
                                       float* outAdvances) const {
        for (uint32_t i = 0; i < count; ++i) {
            outAdvances[i] = GetHorizontalAdvance(glyph_ids[i], paint, fakery);
        }
    }

    virtual void GetBounds(MinikinRect* bounds, uint32_t glyph_id, const MinikinPaint& paint,
                           const FontFakery& fakery) const = 0;

    virtual void GetFontExtent(MinikinExtent* extent, const MinikinPaint& paint,
                               const FontFakery& fakery) const = 0;

    // Returns the font path or an empty string.
    virtual const std::string& GetFontPath() const = 0;

    // Override if font can provide access to raw data
    virtual const void* GetFontData() const { return nullptr; }

    // Override if font can provide access to raw data
    virtual size_t GetFontSize() const { return 0; }

    // Override if font can provide access to raw data.
    // Returns index within OpenType collection
    virtual int GetFontIndex() const { return 0; }

    virtual int GetSourceId() const { return 0; }

    virtual const std::vector<minikin::FontVariation>& GetAxes() const = 0;

    virtual std::shared_ptr<MinikinFont> createFontWithVariation(
            const std::vector<FontVariation>&) const {
        return nullptr;
    }
};

}  // namespace minikin

#endif  // MINIKIN_MINIKIN_FONT_H
