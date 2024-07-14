cmake_minimum_required(VERSION 3.22.1)

project("minikin")
set(MINIKIN_SOURCE_FILES
	BidiUtils.cpp
	CmapCoverage.cpp
	Debug.cpp
	Emoji.cpp
	Font.cpp
	FontCollection.cpp
	FontFamily.cpp
	FontFeatureUtils.cpp
	FontFileParser.cpp
	FontUtils.cpp
	GraphemeBreak.cpp
	GreedyLineBreaker.cpp
	Hyphenator.cpp
	HyphenatorMap.cpp
	Layout.cpp
	LayoutCore.cpp
	LayoutUtils.cpp
	LineBreaker.cpp
	LineBreakerUtil.cpp
	Locale.cpp
	LocaleListCache.cpp
	MeasuredText.cpp
	Measurement.cpp
	MinikinFontFactory.cpp
	MinikinInternal.cpp
	OptimalLineBreaker.cpp
	ScriptUtils.cpp
	SparseBitSet.cpp
	SystemFonts.cpp
	WordBreaker.cpp
)
list(TRANSFORM MINIKIN_SOURCE_FILES PREPEND "${PROJECT_SOURCE_DIR}/src/")
add_library(minikin STATIC ${MINIKIN_SOURCE_FILES})

set(MINIKIN_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
target_include_directories(minikin PRIVATE
	${HARFBUZZ_DIR}/src
	${ICU_DIR}/include
)
target_include_directories(minikin PUBLIC ${MINIKIN_INCLUDE_DIR})
target_link_libraries(minikin harfbuzz icu::icuuc icu::icudata)