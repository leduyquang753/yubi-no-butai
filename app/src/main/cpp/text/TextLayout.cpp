#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <utility>
#include <vector>

#include <minikin/Hyphenator.h>
#include <minikin/LineBreaker.h>
#include <minikin/LineBreakStyle.h>
#include <minikin/MeasuredText.h>
#include <minikin/MinikinPaint.h>
#include <minikin/Range.h>
#include <minikin/U16StringPiece.h>
#include <unicode/ubidi.h>

#include "TextRenderingString.h"

#include "TextLayout.h"

namespace {
	class WidthProvider final: public minikin::LineWidth {
		private:
			const TextLayout::Input *input;
		public:
			WidthProvider(const TextLayout::Input &input): input(&input) {}
			float getAt(const std::size_t line) const override {
				return input->width - (line == 0 ? input->firstLineIndent : 0.f);
			}
			float getMin() const override {
				return 0.f;
			}
	};

	constexpr const std::uint16_t lineEndingCharacters[] = {'\r', '\n', 0};
	constexpr const std::uint16_t rawDummyStringForEmptyParagraph[] = {' '};
	const minikin::U16StringPiece dummyStringForEmptyParagraph(
		rawDummyStringForEmptyParagraph, static_cast<std::uint32_t>(std::size(rawDummyStringForEmptyParagraph))
	);
}

void TextLayout::Input::addRun(
	const TextRenderingStringView runText, const int fontIndex, const float size,
	const float red, const float green, const float blue, const float alpha
) {
	UErrorCode errorCode = U_ZERO_ERROR;
	const auto uBidi = ubidi_openSized(static_cast<std::int32_t>(runText.size()), 0, &errorCode);
	ubidi_setPara(
		uBidi, reinterpret_cast<const UChar*>(runText.data()), static_cast<int32_t>(runText.size()),
		UBIDI_DEFAULT_LTR, nullptr, &errorCode
	);
	const std::int32_t runCount = ubidi_countRuns(uBidi, &errorCode);
	for (std::int32_t i = 0; i != runCount; ++i) {
		std::int32_t runStart, runLength;
		const bool isRtl = ubidi_getVisualRun(uBidi, i, &runStart, &runLength) == UBIDI_RTL;
		const TextRenderingStringView runString(runText.data() + runStart, runLength);
		for (std::size_t subrunStart = 0; subrunStart != runString.size();) {
			std::size_t subrunEnd = runString.find_first_of(lineEndingCharacters, subrunStart);
			const bool isEndOfParagraph = subrunEnd != TextRenderingStringView::npos;
			if (!isEndOfParagraph) subrunEnd = runString.size();
			runs.push_back({
				static_cast<int>(text.size()), static_cast<int>(text.size() + (subrunEnd - subrunStart)),
				fontIndex, size, red, green, blue, alpha, isRtl, isEndOfParagraph
			});
			text.append(runString.begin() + subrunStart, runString.begin() + subrunEnd);
			subrunStart = subrunEnd;
			if (isEndOfParagraph) {
				subrunStart +=
					subrunStart != runString.size() - 1
					&& runString[subrunStart] == '\r' && runString[subrunStart + 1] == '\n'
					? 2 : 1;
			}
		}
	}
}

TextLayout TextLayout::make(const std::vector<minikin::MinikinPaint> &fonts, TextLayout::Input &input) {
	(input.runs.end() - 1)->isEndOfParagraph = true;
	std::vector<Line> lines;
	minikin::MeasuredTextBuilder measuredTextBuilder;
	const WidthProvider widthProvider(input);
	int paragraphStart = 0;
	auto outputRunIterator = input.runs.begin();
	const auto inputEnd = input.runs.end();
	for (auto inputRunIterator = input.runs.begin(); inputRunIterator != inputEnd; ++inputRunIterator) {
		const auto &inputRun = *inputRunIterator;
		{
			auto inputRunPaint = fonts[inputRun.fontIndex];
			inputRunPaint.size = inputRun.size;
			if (inputRun.start != inputRun.end) measuredTextBuilder.addStyleRun(
				inputRun.start - paragraphStart, inputRun.end - paragraphStart, std::move(inputRunPaint),
				static_cast<int>(minikin::LineBreakStyle::Auto), static_cast<int>(minikin::LineBreakWordStyle::Auto),
				false, inputRun.isRtl
			);
		}
		if (!inputRun.isEndOfParagraph) continue;
		// If the paragraph is empty, Minikin's line breaker won't produce any lines. Add a dummy space to
		// force generate one and get its height.
		if (inputRun.end == paragraphStart) {
			// Use the last run which hasn't actually been used.
			auto emptyRunPaint = fonts[inputRun.fontIndex];
			emptyRunPaint.size = inputRun.size;
			measuredTextBuilder.addStyleRun(
				0, static_cast<std::int32_t>(dummyStringForEmptyParagraph.size()), std::move(emptyRunPaint),
				static_cast<int>(minikin::LineBreakStyle::Auto), static_cast<int>(minikin::LineBreakWordStyle::Auto),
				false, inputRun.isRtl
			);
			const auto measuredText
				= measuredTextBuilder.build(dummyStringForEmptyParagraph, false, true, false, nullptr);
			const auto extent = measuredText->getExtent(
				dummyStringForEmptyParagraph, minikin::Range(0, dummyStringForEmptyParagraph.size())
			);
			auto &line = lines.emplace_back();
			line.ascent = -extent.ascent;
			line.descent = extent.descent + input.paragraphSpacing;
			continue;
		}
		const auto textBuffer = toMinikinStringPiece(
			TextRenderingStringView(input.text).substr(paragraphStart, inputRun.end - paragraphStart)
		);
		const auto measuredText = measuredTextBuilder.build(textBuffer, false, true, false, nullptr);
		const auto lineBreakResult = minikin::breakIntoLines(
			textBuffer, minikin::BreakStrategy::HighQuality, minikin::HyphenationFrequency::None, false,
			*measuredText, widthProvider, minikin::TabStops(nullptr, 0, 0.f), false
		);
		int lineStart = paragraphStart;
		const std::size_t lineCount = lineBreakResult.breakPoints.size();
		for (std::size_t lineIndex = 0; lineIndex != lineCount; ++lineIndex) {
			auto &line = lines.emplace_back();
			line.ascent = -lineBreakResult.ascents[lineIndex];
			line.descent = lineBreakResult.descents[lineIndex];
			line.indent = lineIndex == 0 ? input.firstLineIndent : 0.f;
			const int lineEnd = paragraphStart + static_cast<int>(lineBreakResult.breakPoints[lineIndex]);
			while (outputRunIterator != inputEnd) {
				const auto &outputRun = *outputRunIterator;
				const int runStart = std::max(outputRun.start, lineStart);
				const int runEnd = std::min(outputRun.end, lineEnd);
				auto outputRunPaint = fonts[outputRun.fontIndex];
				outputRunPaint.size = outputRun.size;
				line.runs.emplace_back(TextLayout::Run{
					runStart, runEnd,
					outputRun.size, outputRun.red, outputRun.green, outputRun.blue, outputRun.alpha, outputRun.isRtl,
					measuredText->buildLayout(
						textBuffer,
						minikin::Range(runStart - paragraphStart, runEnd - paragraphStart),
						minikin::Range(0, static_cast<std::uint32_t>(input.text.size())),
						outputRunPaint, minikin::StartHyphenEdit::NO_EDIT, minikin::EndHyphenEdit::NO_EDIT
					)
				});
				const bool isEndOfLine = outputRun.end >= lineEnd;
				if (outputRun.end <= lineEnd) ++outputRunIterator;
				if (isEndOfLine) break;
			}
			lineStart = lineEnd;
		}
		(lines.end() - 1)->descent += input.paragraphSpacing;
		paragraphStart = lineStart;
	}
	return {std::move(input.text), std::move(lines)};
}