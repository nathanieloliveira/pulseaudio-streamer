//
// Created by nth on 26/12/2019.
//

#include "pulse_audio_controller.h"
#include <pulse/pulseaudio.h>
#include <iostream>

void pa_event_callback(pa_context *c, const char *name, pa_proplist *p, void *userdata) {
    auto controller = static_cast<pulse_audio_controller *>(userdata);
    controller->on_event(name, p);
}

void pa_state_callback(pa_context *c, void *userdata) {
    auto controller = static_cast<pulse_audio_controller *>(userdata);
    controller->on_state();
}

void pulse_audio_controller::on_event(const char *name, pa_proplist *p) {
    std::cout << "pa_event_callback() name:" << name << std::endl;
}

void pulse_audio_controller::on_state() {
    auto s = pa_context_get_state(context);

    switch (s) {
        case PA_CONTEXT_CONNECTING:
            std::cout << "pa_state_callback(): CONNECTING" << std::endl;
            break;
        case PA_CONTEXT_AUTHORIZING:
            std::cout << "pa_state_callback(): AUTHORIZING" << std::endl;
            break;
        case PA_CONTEXT_SETTING_NAME:
            std::cout << "pa_state_callback(): SETTING_NAME" << std::endl;
            break;

        case PA_CONTEXT_READY: {
            std::cout << "pa_state_callback(): READY" << std::endl;
            state = 1;
            break;
        }

        case PA_CONTEXT_TERMINATED:
            std::cout << "pa_state_callback(): TERMINATED" << std::endl;
            break;

        case PA_CONTEXT_FAILED: {
            auto err = pa_context_errno(context);
            std::cout << "pa_state_callback(): FAILED -- err: " << err << std::endl;
            break;
        }

        default:
            std::cout << "pa_state_callback(): UNKNOWN" << std::endl;
            break;
    }
}

void pulse_audio_controller::connect() {
    if (pa_context_connect(context, static_cast<const char *>(nullptr), static_cast<pa_context_flags>(0),
                           static_cast<pa_spawn_api *>(nullptr)) < 0) {
        std::cout << "pa_context_connect() failed: %s\n" << pa_strerror(pa_context_errno(context));
    }
}

pa_context *pulse_audio_controller::get_context() {
    return context;
}

pulse_audio_controller::pulse_audio_controller(pa_mainloop_api *mainloop_api) : state(0) {
    context = pa_context_new(mainloop_api, "pulseaudio-streamer");
//    pa_signal_init(mainloop_api);
    pa_context_set_event_callback(context, pa_event_callback, this);
    pa_context_set_state_callback(context, pa_state_callback, this);
}

pulse_audio_controller::~pulse_audio_controller() {
    pa_context_unref(context);
}
