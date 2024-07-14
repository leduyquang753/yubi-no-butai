/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "minikin/Font.h"

#include <hb-ot.h>
#include <hb.h>

#include <vector>

#include "FontUtils.h"
#include "LocaleListCache.h"
#include "MinikinInternal.h"
#include "minikin/Constants.h"
#include "minikin/HbUtils.h"
#include "minikin/MinikinFont.h"
#include "minikin/MinikinFontFactory.h"

namespace minikin {

namespace {

// |-------|-------|
//                 X : (1 bit) 1 if weight variation is available, otherwise 0.
//                Y  : (1 bit) 1 if italic variation is available, otherwise 0.
//               I   : (1 bit) 1 for italic, 0 for upright
//     WWWWWWWWWW    : (10 bits) unsigned 10 bits integer for weight value.
inline uint16_t packKey(int wght, int ital) {
    uint16_t res = 0;
    if (wght != -1) {
        res |= 1u;
        res |= static_cast<uint16_t>(wght) << 3;
    }
    if (ital != -1) {
        res |= 1u << 1;
        res |= (ital == 1) ? 1 << 2 : 0;
    }
    return res;
}

}  // namespace

std::shared_ptr<Font> Font::Builder::build() {
    if (mIsWeightSet && mIsSlantSet) {
        // No need to read OS/2 header of the font file.
        return std::shared_ptr<Font>(new Font(std::move(mTypeface), FontStyle(mWeight, mSlant),
                                              prepareFont(mTypeface), mLocaleListId));
    }

    HbFontUniquePtr font = prepareFont(mTypeface);
    FontStyle styleFromFont = analyzeStyle(font);
    if (!mIsWeightSet) {
        mWeight = styleFromFont.weight();
    }
    if (!mIsSlantSet) {
        mSlant = styleFromFont.slant();
    }
    return std::shared_ptr<Font>(new Font(std::move(mTypeface), FontStyle(mWeight, mSlant),
                                          std::move(font), mLocaleListId));
}

Font::Font(BufferReader* reader) : mExternalRefsHolder(nullptr), mTypefaceMetadataReader(nullptr) {
    mStyle = FontStyle(reader);
    mLocaleListId = LocaleListCache::readFrom(reader);
    mTypefaceMetadataReader = *reader;
    MinikinFontFactory::getInstance().skip(reader);
}

void Font::writeTo(BufferWriter* writer) const {
    mStyle.writeTo(writer);
    LocaleListCache::writeTo(writer, mLocaleListId);
    MinikinFontFactory::getInstance().write(writer, baseTypeface().get());
}

Font::Font(Font&& o) noexcept
        : mStyle(o.mStyle),
          mLocaleListId(o.mLocaleListId),
          mTypefaceMetadataReader(o.mTypefaceMetadataReader) {
    mExternalRefsHolder.store(o.mExternalRefsHolder.exchange(nullptr));
}

Font& Font::operator=(Font&& o) noexcept {
    resetExternalRefs(o.mExternalRefsHolder.exchange(nullptr));
    mStyle = o.mStyle;
    mLocaleListId = o.mLocaleListId;
    mTypefaceMetadataReader = o.mTypefaceMetadataReader;
    return *this;
}

Font::~Font() {
    resetExternalRefs(nullptr);
}

void Font::resetExternalRefs(ExternalRefs* refs) {
    ExternalRefs* oldRefs = mExternalRefsHolder.exchange(refs);
    if (oldRefs != nullptr) {
        delete oldRefs;
    }
}

const std::shared_ptr<MinikinFont>& Font::baseTypeface() const {
    return getExternalRefs()->mTypeface;
}

const HbFontUniquePtr& Font::baseFont() const {
    return getExternalRefs()->mBaseFont;
}

const Font::ExternalRefs* Font::getExternalRefs() const {
    // Thread safety note: getExternalRefs() is thread-safe.
    // getExternalRefs() returns the first ExternalRefs set to mExternalRefsHolder.
    // When multiple threads called getExternalRefs() at the same time and
    // mExternalRefsHolder is not set, multiple ExternalRefs may be created,
    // but only one ExternalRefs will be set to mExternalRefsHolder and
    // others will be deleted.
    Font::ExternalRefs* externalRefs = mExternalRefsHolder.load();
    if (externalRefs) return externalRefs;
    // mExternalRefsHolder is null. Try creating an ExternalRefs.
    std::shared_ptr<MinikinFont> typeface =
            MinikinFontFactory::getInstance().create(mTypefaceMetadataReader);
    HbFontUniquePtr font = prepareFont(typeface);
    Font::ExternalRefs* newExternalRefs =
            new Font::ExternalRefs(std::move(typeface), std::move(font));
    // Set the new ExternalRefs to mExternalRefsHolder if it is still null.
    Font::ExternalRefs* expected = nullptr;
    if (mExternalRefsHolder.compare_exchange_strong(expected, newExternalRefs)) {
        return newExternalRefs;
    } else {
        // Another thread has already created and set an ExternalRefs.
        // Delete ours and use theirs instead.
        delete newExternalRefs;
        // compare_exchange_strong writes the stored value into 'expected'
        // when comparison fails.
        return expected;
    }
}

// static
HbFontUniquePtr Font::prepareFont(const std::shared_ptr<MinikinFont>& typeface) {
    const char* buf = reinterpret_cast<const char*>(typeface->GetFontData());
    size_t size = typeface->GetFontSize();
    uint32_t ttcIndex = typeface->GetFontIndex();

    HbBlobUniquePtr blob(hb_blob_create(buf, size, HB_MEMORY_MODE_READONLY, nullptr, nullptr));
    HbFaceUniquePtr face(hb_face_create(blob.get(), ttcIndex));
    HbFontUniquePtr parent(hb_font_create(face.get()));
    hb_ot_font_set_funcs(parent.get());

    uint32_t upem = hb_face_get_upem(face.get());
    hb_font_set_scale(parent.get(), upem, upem);

    HbFontUniquePtr font(hb_font_create_sub_font(parent.get()));
    std::vector<hb_variation_t> variations;
    variations.reserve(typeface->GetAxes().size());
    for (const FontVariation& variation : typeface->GetAxes()) {
        variations.push_back({variation.axisTag, variation.value});
    }
    hb_font_set_variations(font.get(), variations.data(), variations.size());
    return font;
}

// static
FontStyle Font::analyzeStyle(const HbFontUniquePtr& font) {
    HbBlob os2Table(font, MakeTag('O', 'S', '/', '2'));
    if (!os2Table) {
        return FontStyle();
    }

    int weight;
    bool italic;
    if (!::minikin::analyzeStyle(os2Table.get(), os2Table.size(), &weight, &italic)) {
        return FontStyle();
    }
    // TODO: Update weight/italic based on fvar value.
    return FontStyle(static_cast<uint16_t>(weight), static_cast<FontStyle::Slant>(italic));
}

std::unordered_set<AxisTag> Font::getSupportedAxes() const {
    HbBlob fvarTable(baseFont(), MakeTag('f', 'v', 'a', 'r'));
    if (!fvarTable) {
        return std::unordered_set<AxisTag>();
    }
    std::unordered_set<AxisTag> supportedAxes;
    analyzeAxes(fvarTable.get(), fvarTable.size(), &supportedAxes);
    return supportedAxes;
}

HbFontUniquePtr Font::getAdjustedFont(int wght, int ital) const {
    if (wght == -1 && ital == -1) {
        return HbFontUniquePtr(hb_font_reference(baseFont().get()));
    }

    HbFontUniquePtr font(hb_font_create_sub_font(baseFont().get()));
    std::vector<hb_variation_t> variations;
    variations.reserve(baseTypeface()->GetAxes().size());
    for (const FontVariation& variation : baseTypeface()->GetAxes()) {
        if (wght != -1 && variation.axisTag == TAG_wght) {
            continue;  // Add wght axis later
        } else if (ital != -1 && variation.axisTag == TAG_ital) {
            continue;  // Add ital axis later
        } else {
            variations.push_back({variation.axisTag, variation.value});
        }
    }
    if (wght != -1) {
        variations.push_back({TAG_wght, static_cast<float>(wght)});
    }
    if (ital != -1) {
        variations.push_back({TAG_ital, static_cast<float>(ital)});
    }
    hb_font_set_variations(font.get(), variations.data(), variations.size());
    return font;
}

const std::shared_ptr<MinikinFont>& Font::getAdjustedTypeface(int wght, int ital) const {
    return getExternalRefs()->getAdjustedTypeface(wght, ital);
}

const std::shared_ptr<MinikinFont>& Font::ExternalRefs::getAdjustedTypeface(int wght,
                                                                            int ital) const {
    if (wght == -1 && ital == -1) {
        return mTypeface;
    }

    const uint16_t key = packKey(wght, ital);

    std::lock_guard<std::mutex> lock(mMutex);

    std::map<uint16_t, std::shared_ptr<MinikinFont>>::iterator it = mVarTypefaceCache.find(key);
    if (it != mVarTypefaceCache.end()) {
        return it->second;
    }

    std::vector<FontVariation> variations;
    variations.reserve(mTypeface->GetAxes().size());
    for (const FontVariation& variation : mTypeface->GetAxes()) {
        if (wght != -1 && variation.axisTag == TAG_wght) {
            continue;  // Add wght axis later
        } else if (ital != -1 && variation.axisTag == TAG_ital) {
            continue;  // Add ital axis later
        } else {
            variations.push_back({variation.axisTag, variation.value});
        }
    }
    if (wght != -1) {
        variations.push_back({TAG_wght, static_cast<float>(wght)});
    }
    if (ital != -1) {
        variations.push_back({TAG_ital, static_cast<float>(ital)});
    }
    std::shared_ptr<MinikinFont> newTypeface = mTypeface->createFontWithVariation(variations);

    auto [result_iterator, _] =
            mVarTypefaceCache.insert(std::make_pair(key, std::move(newTypeface)));

    return result_iterator->second;
}

}  // namespace minikin
