#ifndef TEXT_TEXTLAYOUT_H
#define TEXT_TEXTLAYOUT_H

#include <utility>
#include <vector>

#include <minikin/Layout.h>
#include <minikin/MinikinPaint.h>

#include "TextRenderingString.h"

class TextLayout final {
	public:
		class Input final {
			private:
				struct InputTextRun {
					int start, end;
					int fontIndex;
					float size;
					float red, green, blue, alpha;
					bool isRtl;
					bool isEndOfParagraph;
				};

				TextRenderingString text;
				std::vector<InputTextRun> runs;
			public:
				float width = 1000.f, firstLineIndent = 0.f, paragraphSpacing = 0.f;

				void addRun(
					const TextRenderingStringView runText, int fontIndex, float size,
					float red, float green, float blue, float alpha
				);

				friend class TextLayout;
		};
		struct Run {
			int start, end;
			float size;
			float red, green, blue, alpha;
			bool isRtl;
			minikin::Layout layout;
		};
		struct Line {
			float ascent, descent, indent;
			std::vector<Run> runs;
		};
	private:
		TextRenderingString text;
		std::vector<Line> lines;

		TextLayout(
			TextRenderingString &&text, std::vector<Line> &&lines
		): text(std::move(text)), lines(std::move(lines)) {}
	public:
		static TextLayout make(const std::vector<minikin::MinikinPaint> &fonts, Input &input);

		const TextRenderingString& getText() const {
			return text;
		}
		const std::vector<Line> getLines() const {
			return lines;
		}
};

#endif // TEXT_TEXTLAYOUT_H