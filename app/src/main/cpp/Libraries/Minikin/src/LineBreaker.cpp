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

#include "minikin/LineBreaker.h"

#include "GreedyLineBreaker.h"
#include "OptimalLineBreaker.h"

namespace minikin {

LineBreakResult breakIntoLines(const U16StringPiece& textBuffer, BreakStrategy strategy,
                               HyphenationFrequency frequency, bool justified,
                               const MeasuredText& measuredText, const LineWidth& lineWidth,
                               const TabStops& tabStops, bool useBoundsForWidth) {
    if (strategy == BreakStrategy::Greedy || textBuffer.hasChar(CHAR_TAB)) {
        return breakLineGreedy(textBuffer, measuredText, lineWidth, tabStops,
                               frequency != HyphenationFrequency::None, useBoundsForWidth);
    } else {
        return breakLineOptimal(textBuffer, measuredText, lineWidth, strategy, frequency, justified,
                                useBoundsForWidth);
    }
}

}  // namespace minikin
