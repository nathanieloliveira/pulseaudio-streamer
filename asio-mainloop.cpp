//
// Created by nth on 03/12/2019.
//

#include <list>
#include "asio-mainloop.h"
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <iostream>

using namespace boost::asio;

struct wait_flags {
    bool read;
    bool write;
    bool error;

    wait_flags() : read(false), write(false), error(false) {}
};

void pa_events_to_asio(pa_io_event_flags_t events, wait_flags &wait_flags) {
    wait_flags.read = events & PA_IO_EVENT_INPUT;
    wait_flags.write = events & PA_IO_EVENT_OUTPUT;
    wait_flags.error = events & PA_IO_EVENT_ERROR || events & PA_IO_EVENT_HANGUP;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-signed-bitwise"

pa_io_event_flags_t pa_events_to_asio(wait_flags &wait_flags) {
    return static_cast<pa_io_event_flags_t>((wait_flags.read ? PA_IO_EVENT_INPUT : 0) |
                                            (wait_flags.write ? PA_IO_EVENT_OUTPUT : 0) |
                                            (wait_flags.error ? PA_IO_EVENT_ERROR : 0));
}

#pragma clang diagnostic pop

struct pa_io_event {
    posix::stream_descriptor sd;
    int descriptor;
    wait_flags flags;

    pa_mainloop_api *ea;
    pa_io_event_flags_t events;
    pa_io_event_cb_t callback;
    void *user_data;

    pa_io_event_destroy_cb_t destroy_callback = nullptr;

    bool destroyed = false;

    pa_io_event(io_context *context, pa_mainloop_api *mainloop, int fd, pa_io_event_flags_t events, pa_io_event_cb_t cb,
                void *userdata)
            : sd(*context, fd), descriptor(fd), flags(), ea(mainloop), events(events), callback(cb),
              user_data(userdata) {
        std::cout << "io_event_new fd: " << descriptor << std::endl;
        pa_events_to_asio(events, flags);
        context->post([this, events]{
            this->enable(events);
        });
    }

    ~pa_io_event() {
        std::cout << "io_event_free fd: " << descriptor << std::endl;
        destroyed = true;
        if (destroy_callback != nullptr) {
            std::cout << "io_event_destroy_callback fd: " << descriptor << std::endl;
            destroy_callback(ea, this, user_data);
        }
    }

    void enable(pa_io_event_flags_t events) {
        std::cout << "io_event_enable fd: " << descriptor << std::endl;
        if (destroyed) {
            return;
        }
        sd.cancel();
        this->events = events;
        pa_events_to_asio(events, flags);
        if (flags.read) {
            wait_read();
        }
        if (flags.write) {
            wait_write();
        }
        if (flags.error) {
            wait_error();
        }
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "InfiniteRecursion"

    void wait_read() {
        if (destroyed) {
            return;
        }
        sd.async_wait(posix::descriptor_base::wait_read, [this](boost::system::error_code error) {
            if (!error) {
                std::cout << "io_read_event fd: " << descriptor << std::endl;
                callback(ea, this, descriptor, PA_IO_EVENT_INPUT, user_data);
            }
        });
    }

    void wait_write() {
        if (destroyed) {
            return;
        }
        sd.async_wait(posix::descriptor_base::wait_write, [this](boost::system::error_code error) {
            if (!error) {
                std::cout << "io_write_event fd: " << descriptor << std::endl;
                callback(ea, this, descriptor, PA_IO_EVENT_OUTPUT, user_data);
            }
        });
    }

    void wait_error() {
        if (destroyed) {
            return;
        }
        sd.async_wait(posix::descriptor_base::wait_error, [this](boost::system::error_code error) {
            if (!error) {
                std::cout << "io_error_event fd: " << descriptor << std::endl;
                callback(ea, this, descriptor, PA_IO_EVENT_ERROR, user_data);
            }
        });
    }

#pragma clang diagnostic pop

    void set_destroy_callback(pa_io_event_destroy_cb_t cb) {
        std::cout << "io_event_set_destroy_callback fd: " << descriptor << std::endl;
        destroy_callback = cb;
    }

};

pa_io_event *asio_io_new(pa_mainloop_api *a, int fd, pa_io_event_flags_t events, pa_io_event_cb_t cb, void *userdata) {
    auto context = static_cast<io_context *>(a->userdata);
    return new pa_io_event(context, a, fd, events, cb, userdata);
}

void asio_io_enable(pa_io_event *e, pa_io_event_flags_t events) {
    e->enable(events);
}

void asio_io_free(pa_io_event *e) {
    delete e;
}

void asio_io_set_destroy(pa_io_event *e, pa_io_event_destroy_cb_t cb) {
    e->set_destroy_callback(cb);
}

template<class time_type = boost::posix_time::ptime>
boost::posix_time::ptime from_timeval(const struct timeval *tv) {
    typedef typename time_type::time_duration_type time_duration_type;
    typedef typename time_type::date_type date_type;
    typedef typename time_duration_type::rep_type resolution_traits_type;

    std::time_t t = tv->tv_sec;
    boost::uint32_t sub_sec = tv->tv_usec;

    std::tm curr{};
    std::tm *curr_ptr = boost::date_time::c_time::gmtime(&t, &curr);
    date_type d(static_cast< typename date_type::year_type::value_type >(curr_ptr->tm_year + 1900),
                static_cast< typename date_type::month_type::value_type >(curr_ptr->tm_mon + 1),
                static_cast< typename date_type::day_type::value_type >(curr_ptr->tm_mday));
    //The following line will adjust the fractional second tick in terms
    //of the current time system.  For example, if the time system
    //doesn't support fractional seconds then res_adjust returns 0
    //and all the fractional seconds return 0.
    int adjust = static_cast< int >(resolution_traits_type::res_adjust() / 1000000);
    time_duration_type td(static_cast< typename time_duration_type::hour_type >(curr_ptr->tm_hour),
                          static_cast< typename time_duration_type::min_type >(curr_ptr->tm_min),
                          static_cast< typename time_duration_type::sec_type >(curr_ptr->tm_sec),
                          sub_sec * adjust);
    return time_type(d, td);
}

static int time_event_id = 0;

struct pa_time_event {
    int id;
    pa_mainloop_api *mainloop;
    deadline_timer timer;
    const struct timeval *time;
    pa_time_event_cb_t callback;
    void *data;
    pa_time_event_destroy_cb_t destroy_cb;

    pa_time_event(pa_mainloop_api *a, const struct timeval *tv, pa_time_event_cb_t cb, void *userdata)
            : id(time_event_id++),
              mainloop(a),
              timer(static_cast<io_context *>(a->userdata)->get_executor(), from_timeval(tv)),
              time(tv),
              callback(cb),
              data(userdata),
              destroy_cb(nullptr) {
        std::cout << "time_new id: " << id << std::endl;
        timer.async_wait([this](boost::system::error_code error) {
            if (!error) {
                std::cout << "time_callback id: " << id << std::endl;
                callback(mainloop, this, time, data);
            }
        });
    }

    ~pa_time_event() {
        std::cout << "time_free id: " << id << std::endl;
        cancel();
        if (destroy_cb != nullptr) {
            std::cout << "time_destroy_callback id: " << id << std::endl;
            destroy_cb(mainloop, this, data);
        }
    }

    void set_destroy_callback(pa_time_event_destroy_cb_t cb) {
        std::cout << "time_set_destroy_callback id: " << id << std::endl;
        destroy_cb = cb;
    }

    void cancel() {
        std::cout << "time_cancel id: " << id << std::endl;
        timer.cancel();
    }

    void restart(const struct timeval *tv) {
        std::cout << "time_set_restart id: " << id << std::endl;
        timer.expires_at(from_timeval(tv));
    }
};


/** Create a new timer event source object for the specified Unix time */
pa_time_event *asio_time_new(pa_mainloop_api *a, const struct timeval *tv, pa_time_event_cb_t cb, void *userdata) {
    return new pa_time_event(a, tv, cb, userdata);
}

/** Restart a running or expired timer event source with a new Unix time */
void asio_time_restart(pa_time_event *e, const struct timeval *tv) {
    e->restart(tv);
}

/** Free a deferred timer event source object */
void asio_time_free(pa_time_event *e) {
    delete e;
}

/** Set a function that is called when the timer event source is destroyed. Use this to free the userdata argument if required */
void asio_time_set_destroy(pa_time_event *e, pa_time_event_destroy_cb_t cb) {
    e->set_destroy_callback(cb);
}

static int defer_id = 0;

struct pa_defer_event {
    int id;
    pa_mainloop_api *mainloop;
    pa_defer_event_cb_t callback;
    pa_defer_event_destroy_cb_t destroy_cb;
    void *data;
    bool enabled;

    pa_defer_event(pa_mainloop_api *a, pa_defer_event_cb_t cb, void *userdata) :
            id(defer_id++),
            mainloop(a),
            callback(cb),
            destroy_cb(nullptr),
            data(userdata),
            enabled(true) {
        std::cout << "defer_new id: " << id << std::endl;
        static_cast<io_context *>(mainloop->userdata)->post(
                [this]() {
                    if (this->enabled) {
                        std::cout << "defer_callback id: " << id << " enabled: " << this->enabled << std::endl;
                        callback(mainloop, this, data);
                    }
                });
    }

    ~pa_defer_event() {
        std::cout << "defer_free id: " << id << std::endl;
        if (destroy_cb != nullptr) {
            std::cout << "defer_destroy_callback id: " << id << std::endl;
            destroy_cb(mainloop, this, data);
        }
    }

    void set_enabled(int b) {
        std::cout << "defer_set_enabled id: " << id << " enabled: " << enabled << " new: " << b << std::endl;
        enabled = b != 0;
    }

    void set_destroy_cb(pa_defer_event_destroy_cb_t cb) {
        std::cout << "defer_set_destroy_callback id: " << id << std::endl;
        destroy_cb = cb;
    }
};

/** Create a new deferred event source object */
pa_defer_event *asio_defer_new(pa_mainloop_api *a, pa_defer_event_cb_t cb, void *userdata) {
    return new pa_defer_event(a, cb, userdata);
}

/** Enable or disable a deferred event source temporarily */
void asio_defer_enable(pa_defer_event *e, int b) {
    e->set_enabled(b);
}

/** Free a deferred event source object */
void asio_defer_free(pa_defer_event *e) {
    delete e;
}

/** Set a function that is called when the deferred event source is destroyed. Use this to free the userdata argument if required */
void asio_defer_set_destroy(pa_defer_event *e, pa_defer_event_destroy_cb_t cb) {
    e->set_destroy_cb(cb);
}

/** Exit the main loop and return the specified retval*/
void asio_quit(pa_mainloop_api *a, int retval) {
    std::cout << "mainloop_exit!" << std::endl;
    static_cast<io_context *>(a->userdata)->stop();
    exit(retval);
}

std::shared_ptr<pa_mainloop_api> get_asio_mainloop(io_context *context) {
    auto vtable = std::make_shared<pa_mainloop_api>();
    vtable->userdata = context;
    vtable->io_new = asio_io_new;
    vtable->io_enable = asio_io_enable;
    vtable->io_free = asio_io_free;
    vtable->io_set_destroy = asio_io_set_destroy;
    vtable->time_new = asio_time_new;
    vtable->time_restart = asio_time_restart;
    vtable->time_free = asio_time_free;
    vtable->time_set_destroy = asio_time_set_destroy;
    vtable->defer_new = asio_defer_new;
    vtable->defer_enable = asio_defer_enable;
    vtable->defer_free = asio_defer_free;
    vtable->defer_set_destroy = asio_defer_set_destroy;
    vtable->quit = asio_quit;
    return vtable;
}