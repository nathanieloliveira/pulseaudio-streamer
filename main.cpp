#include <iostream>
#include <thread>
#include <chrono>
#include <pulse/simple.h>
#include <boost/asio.hpp>
#include "asio-mainloop.h"
#include "pulse_audio_controller.h"

int main() {
    std::cout << "Hello World" << std::endl;

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16NE;
    ss.channels = 2;
    ss.rate = 44100;

    auto stream = pa_simple_new(NULL, "test", PA_STREAM_PLAYBACK, NULL, "Music", &ss, NULL, NULL, NULL);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    pa_simple_free(stream);

    auto context = boost::asio::io_context();
    auto asio_mainloop = get_asio_mainloop(&context);
    pulse_audio_controller controller(asio_mainloop.get());

    context.post([&controller]() {
        controller.connect();
    });

    /*auto acceptor = boost::asio::ip::tcp::acceptor(context, boost::asio::ip::tcp::v4(), 12839);
    auto endpoint = boost::asio::ip::tcp::endpoint();
    auto socket = boost::asio::ip::tcp::socket(context);
    acceptor.async_accept(socket, [](boost::system::error_code ec) {

    });*/

    return context.run();
}
