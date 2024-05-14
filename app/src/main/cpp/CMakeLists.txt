cmake_minimum_required(VERSION 3.22.1)
include(FetchContent)

project("yubinobutai")

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
set(FFMPEG_DIR "${PROJECT_SOURCE_DIR}/../../../../FFmpeg/${ANDROID_ABI}")
add_library(ffmpeg::avcodec SHARED IMPORTED)
set_target_properties(ffmpeg::avcodec PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavcodec.so)
add_library(ffmpeg::avformat SHARED IMPORTED)
set_target_properties(ffmpeg::avformat PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavformat.so)
add_library(ffmpeg::avutil SHARED IMPORTED)
set_target_properties(ffmpeg::avutil PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavutil.so)
add_library(ffmpeg::swresample SHARED IMPORTED)
set_target_properties(ffmpeg::swresample PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libswresample.so)

set(CMAKE_SHARED_LINKER_FLAGS
	"${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

target_include_directories(yubinobutai PUBLIC
	${PROJECT_SOURCE_DIR}
	${FFMPEG_DIR}/include
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
	glm::glm
	jnigraphics
	log
	oboe::oboe
)