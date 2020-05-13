#include "engine.hpp"

#include <cerrno>

uint32_t service_thread_t::serve_queue(service_callback_t& ctx,
                                       message_queue_t& mq) noexcept(false) {
    auto user_data = static_cast<uintptr_t>(get_current_thread_id());
    auto ec = ctx.on_begin(user_data);
    while (ec == 0) {
        auto msg = uintptr_t{};
        bool widowed = false;
        ec = mq.recv(msg, widowed);
        if (ec || widowed) {
            ctx.on_close(user_data, ec);
            break;
        }
        ec = ctx.on_message(user_data, msg);
    }
    return ctx.on_end(user_data), ec;
};

service_thread_t::service_thread_t(service_callback_t& ctx) noexcept(false)
    : mq{}, rdv{spawn(ctx, this->mq)} {
    // ...
}

service_thread_t::~service_thread_t() noexcept(false) {
    if (rdv.valid()) // if not joined, join at this moment
        rdv.get();
}

uint32_t service_thread_t::send(uintptr_t message) noexcept {
    if (rdv.valid() == false)
        return EBADF; // send to joined proxy
    return mq.send(message);
}

uint32_t
service_thread_t::join(std::chrono::milliseconds timeout) noexcept(false) {
    // already joined
    if (rdv.valid() == false) {
        return ENOTRECOVERABLE;
    }
    // close the message queue
    if (const auto ec = mq.widow()) {
        if (ec != EBADF)
            return ec;
        // for EBADF, retry because of previous timeout
        // ... wait/get again ...
    }
    // wait/get for the handler thread
    switch (rdv.wait_for(timeout)) {
    case std::future_status::timeout:
        return EINPROGRESS;
    case std::future_status::deferred:
    case std::future_status::ready:
        return rdv.get();
    }
}
