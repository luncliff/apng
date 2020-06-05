#include "service.hpp"

using namespace std;

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

bool service_thread_t::alive() noexcept {
    return rdv.valid();
}

uint32_t service_thread_t::send(uintptr_t message) noexcept {
    if (rdv.valid() == false)
        return EBADF; // send to joined proxy
    return mq.send(message);
}
uint32_t service_thread_t::shutdown() noexcept {
    return mq.widow();
}

uint32_t service_thread_t::join(chrono::milliseconds timeout) noexcept(false) {
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
    case future_status::timeout:
        return EINPROGRESS;
    case future_status::deferred:
    case future_status::ready:
        return rdv.get();
    }
}

using namespace std::experimental;

struct worker_impl_t : service_callback_t {
    uint32_t on_begin(uintptr_t& user_data) noexcept override {
        return 0;
    }
    void on_end(uintptr_t user_data) noexcept override {
        return;
    }
    void on_close(uintptr_t user_data, uint32_t ec) noexcept override {
        return;
    }
    uint32_t on_message(uintptr_t& user_data, uintptr_t msg) noexcept override {
        if (msg == 0)
            return EINVAL;
        auto task = coroutine_handle<void>::from_address( //
            reinterpret_cast<void*>(msg));
        try {
            if (task.done() == false)
                task.resume();
        } catch (const error_code& ex) {
            return ex.value();
        } catch (const system_error& ex) {
            return ex.code().value();
        } catch (const exception& ex) {
            return EXIT_FAILURE;
        }
        return 0;
    }
};

async_service_t::async_service_t() noexcept(false)
    : service{make_unique<worker_impl_t>()}, worker{*service} {
    // ...
}
async_service_t::~async_service_t() noexcept(false) {
    if (int ec = worker.shutdown())
        throw system_error{ec, system_category()};
}

bool async_service_t::ready() noexcept {
    // if worker is not alive, we can't schedule the task.
    // return `true` so the job can be resumed immediately
    return worker.alive() == false;
}

void async_service_t::suspend(task_handle_t coro) //
    noexcept(false) {
    const auto msg = reinterpret_cast<uintptr_t>(coro.address());
    if (int ec = worker.send(msg))
        throw system_error{ec, system_category()};
}

uint64_t async_service_t::resume() noexcept {
    return get_current_thread_id();
}
