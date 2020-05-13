#pragma once
#include <future>
#if __has_include(<unistd.h>)
#include <pthread.h>
#include <unistd.h>

class message_queue_t final {
    int fds[2]{};

  public:
    message_queue_t() noexcept(false);
    ~message_queue_t() noexcept;
    message_queue_t(const message_queue_t&) = delete;
    message_queue_t(message_queue_t&&) = delete;
    message_queue_t& operator=(const message_queue_t&) = delete;
    message_queue_t& operator=(message_queue_t&&) = delete;

  public:
    uint32_t send(uintptr_t user_data) noexcept;
    uint32_t recv(uintptr_t& user_data, bool& widowed) noexcept;
    uint32_t widow() noexcept;
};

class notification_t final {
    int fds[2]{};

  public:
    notification_t() noexcept(false);
    ~notification_t() noexcept;
    notification_t(const notification_t&) = delete;
    notification_t(notification_t&&) = delete;
    notification_t& operator=(const notification_t&) = delete;
    notification_t& operator=(notification_t&&) = delete;

    static_assert(sizeof(pthread_t) == sizeof(uintptr_t));

  public:
    uint32_t signal() noexcept;
    uint32_t wait() noexcept;
};

#elif __has_include(<winrt/Windows.Foundation.h>) // 10.0.17134.0 or later
#include <Windows.h>

class message_queue_t final {
    HANDLE cp;

  public:
    message_queue_t() noexcept(false);
    ~message_queue_t() noexcept;
    message_queue_t(const message_queue_t&) = delete;
    message_queue_t(message_queue_t&&) = delete;
    message_queue_t& operator=(const message_queue_t&) = delete;
    message_queue_t& operator=(message_queue_t&&) = delete;

    static_assert(sizeof(uintptr_t) == sizeof(LPOVERLAPPED));

  public:
    uint32_t send(uintptr_t user_data) noexcept;
    uint32_t recv(uintptr_t& user_data, bool& widowed) noexcept;
    uint32_t widow() noexcept;
};

class notification_t final {
    HANDLE const handle;

  public:
    notification_t() noexcept(false);
    ~notification_t() noexcept;

    uint32_t wait() noexcept;
    uint32_t signal() noexcept;
};

#endif

uint64_t get_current_thread_id() noexcept;

class service_callback_t {
  public:
    virtual ~service_callback_t() noexcept = default;

  public:
    virtual uint32_t on_begin(uintptr_t& user_data) noexcept = 0;
    virtual void on_end(uintptr_t user_data) noexcept = 0;
    virtual void on_close(uintptr_t user_data, uint32_t ec) noexcept = 0;
    virtual uint32_t on_message(uintptr_t& user_data,
                                uintptr_t msg) noexcept = 0;
};

class service_thread_t final {
    message_queue_t mq;
    std::future<uint32_t> rdv; // rendezvous

  private:
    static uint32_t serve_queue(service_callback_t& ctx,
                                message_queue_t& mq) noexcept(false);

    static auto spawn(service_callback_t& ctx,
                      message_queue_t& mq) noexcept(false)
        -> std::future<uint32_t> {
        auto subroutine = [&]() mutable noexcept(false) -> uint32_t {
            return serve_queue(ctx, mq);
        };
        return std::async(std::launch::async, subroutine);
    }

  public:
    service_thread_t(service_callback_t& ctx) noexcept(false);
    ~service_thread_t() noexcept(false);

  public:
    uint32_t send(uintptr_t message) noexcept;
    uint32_t join(std::chrono::milliseconds timeout) noexcept(false);
};
