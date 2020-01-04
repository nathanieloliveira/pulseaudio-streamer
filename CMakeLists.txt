cmake_minimum_required(VERSION 3.8)
project(pulseaudio-streamer)

include(${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake)
conan_basic_setup()

find_package(PulseAudio)

add_executable(pulseaudio-streamer main.cpp asio-mainloop.cpp asio-mainloop.h pulse_audio_controller.cpp pulse_audio_controller.h)
set_property(TARGET pulseaudio-streamer PROPERTY CXX_STANDARD 17)
target_link_libraries(pulseaudio-streamer ${CONAN_LIBS} pulse pulse-simple)
