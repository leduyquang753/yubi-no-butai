/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef MINIKIN_FAMILY_VARIANT_H
#define MINIKIN_FAMILY_VARIANT_H

#include <cstdint>
#include <ostream>

namespace minikin {

// Must be the same value as FontConfig.java
enum class FamilyVariant : uint8_t {
    DEFAULT = 0,  // Must be the same as FontConfig.VARIANT_DEFAULT
    COMPACT = 1,  // Must be the same as FontConfig.VARIANT_COMPACT
    ELEGANT = 2,  // Must be the same as FontConfig.VARIANT_ELEGANT
};

inline std::ostream& operator<<(std::ostream& os, const FamilyVariant& variant) {
    switch (variant) {
        case FamilyVariant::DEFAULT:
            return os << "default";
        case FamilyVariant::COMPACT:
            return os << "compact";
        case FamilyVariant::ELEGANT:
            return os << "elegant";
        default:
            return os << "[UNKNOWN]";
    }
}

}  // namespace minikin

#endif  // MINIKIN_FAMILY_VARIANT_H
