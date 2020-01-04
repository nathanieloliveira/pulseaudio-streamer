//
// Created by nth on 03/12/2019.
//

#ifndef PULSEAUDIO_STREAMER_ASIO_MAINLOOP_H
#define PULSEAUDIO_STREAMER_ASIO_MAINLOOP_H

#include "memory"
#include <pulse/mainloop-api.h>
#include <boost/asio.hpp>

std::shared_ptr<pa_mainloop_api> get_asio_mainloop(boost::asio::io_context* context);

#endif //PULSEAUDIO_STREAMER_ASIO_MAINLOOP_H
