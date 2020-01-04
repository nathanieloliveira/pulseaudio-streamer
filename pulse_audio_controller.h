//
// Created by nth on 26/12/2019.
//

#ifndef PULSEAUDIO_STREAMER_PULSE_AUDIO_CONTROLLER_H
#define PULSEAUDIO_STREAMER_PULSE_AUDIO_CONTROLLER_H

#include <pulse/pulseaudio.h>

class pulse_audio_controller {
    pa_context *context;
    int state;
public:
    explicit pulse_audio_controller(pa_mainloop_api *mainloop_api);
    ~pulse_audio_controller();
    void connect();

    void on_event(const char *name, pa_proplist *p);
    void on_state();

    pa_context * get_context();
};


#endif //PULSEAUDIO_STREAMER_PULSE_AUDIO_CONTROLLER_H
