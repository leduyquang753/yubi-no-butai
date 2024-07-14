#ifndef TEXT_TEXTRENDERINGSTRING_H
#define TEXT_TEXTRENDERINGSTRING_H

#include <cstdint>
#include <string>
#include <string_view>

#include <minikin/U16StringPiece.h>

using TextRenderingString = std::basic_string<std::uint16_t>;
using TextRenderingStringView = std::basic_string_view<std::uint16_t>;

template<typename T>
TextRenderingString toTextRenderingString(const std::basic_string_view<T> view) {
	return {view.begin(), view.end()};
}

inline minikin::U16StringPiece toMinikinStringPiece(const TextRenderingStringView view) {
	return {view.data(), static_cast<std::uint32_t>(view.size())};
}

#endif // TEXT_TEXTRENDERINGSTRING_H