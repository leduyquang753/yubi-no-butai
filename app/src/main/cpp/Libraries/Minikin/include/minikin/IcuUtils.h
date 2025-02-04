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

#ifndef MINIKIN_ICUUTILS_H
#define MINIKIN_ICUUTILS_H

#include <unicode/ubrk.h>
#include <memory>

namespace minikin {

struct IcuUbrkDeleter {
    void operator()(UBreakIterator* v) { ubrk_close(v); }
};

using IcuUbrkUniquePtr = std::unique_ptr<UBreakIterator, IcuUbrkDeleter>;

}  // namespace minikin

#endif  // MINIKIN_ICUUTILS_H
