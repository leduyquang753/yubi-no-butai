cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
include(FetchContent)

project("yubinobutai")
set(CMAKE_CXX_STANDARD 20 REQUIRED)
set(ANDROID_PROJECT_ROOT "${PROJECT_SOURCE_DIR}/../../../..")
set(COMPILED_LIBRARIES_DIR "${ANDROID_PROJECT_ROOT}/CompiledLibraries")
set(LIBRARIES_DIR "${PROJECT_SOURCE_DIR}/Libraries")

include("SourceFiles.cmake")
list(TRANSFORM YUBINOBUTAI_SOURCE_FILES PREPEND "${PROJECT_SOURCE_DIR}/")
add_library(yubinobutai SHARED ${YUBINOBUTAI_SOURCE_FILES})

find_package(game-activity REQUIRED CONFIG)
find_package(oboe REQUIRED CONFIG)

FetchContent_Declare(glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e
)
FetchContent_MakeAvailable(glm)
FetchContent_Declare(libq
	GIT_REPOSITORY https://github.com/cycfi/q.git
	GIT_TAG f14d5ff77185d97f37ac77ac50c74fb2bc146231
)
set(Q_BUILD_EXAMPLES OFF)
set(Q_BUILD_TEST OFF)
set(Q_BUILD_IO OFF)
FetchContent_MakeAvailable(libq)

set(FFMPEG_DIR "${COMPILED_LIBRARIES_DIR}/FFmpeg/${ANDROID_ABI}")
add_library(ffmpeg::avcodec SHARED IMPORTED)
set_target_properties(ffmpeg::avcodec PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/libavcodec.so)
add_library(ffmpeg::avformat SHARED IMPORTED)
set_target_properties(ffmpeg::avformat PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/libavformat.so)
add_library(ffmpeg::avutil SHARED IMPORTED)
set_target_properties(ffmpeg::avutil PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/libavutil.so)
add_library(ffmpeg::swresample SHARED IMPORTED)
set_target_properties(ffmpeg::swresample PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/libswresample.so)

set(HARFBUZZ_DIR "${COMPILED_LIBRARIES_DIR}/HarfBuzz/${ANDROID_ABI}")
add_library(freetype SHARED IMPORTED)
set_target_properties(freetype PROPERTIES IMPORTED_LOCATION ${HARFBUZZ_DIR}/libfreetype.so)
add_library(png SHARED IMPORTED)
set_target_properties(png PROPERTIES IMPORTED_LOCATION ${HARFBUZZ_DIR}/libpng16.so)
add_library(harfbuzz SHARED IMPORTED)
set_target_properties(harfbuzz PROPERTIES IMPORTED_LOCATION ${HARFBUZZ_DIR}/libharfbuzz.so)

set(ICU_DIR "${COMPILED_LIBRARIES_DIR}/ICU/${ANDROID_ABI}")
add_library(icu::icuuc STATIC IMPORTED)
set_target_properties(icu::icuuc PROPERTIES IMPORTED_LOCATION ${ICU_DIR}/libicuuc.a)
add_library(icu::icudata STATIC IMPORTED)
set_target_properties(icu::icudata PROPERTIES IMPORTED_LOCATION ${ICU_DIR}/libicudata.a)

add_subdirectory("Libraries/Minikin")

set(CMAKE_SHARED_LINKER_FLAGS
	"${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

target_include_directories(yubinobutai PRIVATE
	${PROJECT_SOURCE_DIR}
	${FFMPEG_DIR}/include
	${HARFBUZZ_DIR}/include
	${HARFBUZZ_DIR}/src
	${ICU_DIR}/include
	${LIBRARIES_DIR}
)
target_compile_options(yubinobutai PUBLIC -fno-omit-frame-pointer)
target_link_libraries(yubinobutai
	game-activity::game-activity_static

	EGL
	GLESv3
	android
	ffmpeg::avcodec
	ffmpeg::avformat
	ffmpeg::avutil
	ffmpeg::swresample
	freetype
	glm::glm
	harfbuzz
	icu::icuuc
	icu::icudata
	jnigraphics
	libq
	log
	minikin
	oboe::oboe
	png
)