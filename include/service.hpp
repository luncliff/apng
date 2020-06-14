/**
 * @brief Platform's services with their System API
 */
#pragma once
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <future>
#include <memory>
#include <system_error>

namespace fs = std::filesystem;

#if __has_include(<experimental/coroutine>) // C++ 20 Coroutines TS
#include <experimental/coroutine>
#endif
#if __has_include(<unistd.h>) // POSIX.1-2017
#include <pthread.h>
#include <unistd.h>
#elif __has_include(<winrt/Windows.Foundation.h>) // 10.0.17134.0 or later
#include <Windows.h>
#endif

#if defined(_WIN32)

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

#else

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

  public:
    static uint32_t serve_queue(service_callback_t& ctx,
                                message_queue_t& mq) noexcept(false);

  private:
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
    bool alive() noexcept;
    uint32_t send(uintptr_t message) noexcept;
    uint32_t shutdown() noexcept;
    uint32_t join(std::chrono::milliseconds timeout) noexcept(false);
};

#if __has_include(<experimental/coroutine>) // C++ 20 Coroutines TS

class async_service_t final {
  public:
    using task_handle_t = std::experimental::coroutine_handle<void>;

  private:
    std::unique_ptr<service_callback_t> service;
    service_thread_t worker;

  public:
    async_service_t() noexcept(false);
    ~async_service_t() noexcept(false);

  private:
    bool ready() noexcept;
    void suspend(task_handle_t coro) noexcept(false);
    uint64_t resume() noexcept;

  public:
    bool await_ready() noexcept {
        return this->ready();
    }
    void await_suspend(task_handle_t coro) noexcept(false) {
        return this->suspend(coro);
    }
    uint64_t await_resume() noexcept {
        return this->resume();
    }
};

// mock C++/WinRT fire_and_forget
struct fire_and_forget final {
    struct promise_type {
        auto initial_suspend() noexcept {
            return std::experimental::suspend_never{};
        }
        auto final_suspend() noexcept {
            return std::experimental::suspend_never{};
        }
        auto get_return_object() noexcept -> fire_and_forget {
            return {};
        }
        void unhandled_exception() {
            std::terminate();
        }
        void return_void() noexcept {
            return;
        }
    };
};
#endif

auto create(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto read(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]>;
auto read_all(const fs::path& p, size_t& fsize) -> std::unique_ptr<std::byte[]>;
