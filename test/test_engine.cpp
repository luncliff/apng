#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>
#include <gsl/gsl>

#include <engine.hpp>

using namespace std;

TEST_CASE("message queue") {
    message_queue_t q{};

    SECTION("multiple widow") {
        REQUIRE(q.widow() == 0);
#if defined(_WIN32)
        if (q.widow() == 0)
            WARN("should return non-zero");
#else
        REQUIRE_FALSE(q.widow() == 0); // already closed
#endif
    }
    SECTION("send then recv") {
        uintptr_t msg = 0xBEAF; // 48815
        REQUIRE(q.send(msg) == 0);

        msg = 0;
        bool widowed = true;
        REQUIRE(q.recv(msg, widowed) == 0);
        REQUIRE(msg == 0xBEAF);
        REQUIRE_FALSE(widowed);
    }

    SECTION("mt-safe") {
        auto f = async(launch::async, [&q]() noexcept(false) -> size_t {
            uintptr_t msg{};
            bool widowed = false;
            size_t count = 0;
            while (widowed == false) {
                if (auto ec = q.recv(msg, widowed)) {
                    fprintf(stdout, "errored: %d %s\n", ec, strerror(ec));
                    break;
                }
                fprintf(stdout, "recv   : %u\n", msg);
                ++count;
                msg = 0;
            }
            if (widowed) {
                fprintf(stdout, "widowed: %u\n", msg); // expect 0x00
            }
            return count;
        });

        uintptr_t msg = 0xBEAF;
        REQUIRE(q.send(msg) == 0);
        REQUIRE(q.send(msg) == 0);
        REQUIRE(q.send(msg) == 0);
        REQUIRE(q.widow() == 0); // becomes 4th message

        REQUIRE(f.get() == 4);
    }
}

TEST_CASE("service callback") {
    struct impl_t : public service_callback_t {
        bool b = false, c = false;
        uintptr_t msg = UINT32_MAX;
        uint32_t ec = UINT16_MAX;

      private:
        uint32_t on_begin(uintptr_t& u) noexcept override {
            fprintf(stdout, "%-10s %u\n", __func__, u);
            fprintf(stdout, "  thread   %u\n", get_current_thread_id());
            b = true;
            return 0;
        }
        void on_end(uintptr_t u) noexcept override {
            fprintf(stdout, "%-10s %u\n", __func__, u);
            fprintf(stdout, "  thread   %u\n", get_current_thread_id());
        }
        void on_close(uintptr_t u, uint32_t _ec) noexcept override {
            fprintf(stdout, "%-10s %u\n", __func__, u);
            fprintf(stdout, "  thread   %u\n", get_current_thread_id());
            fprintf(stdout, "  error    %u\n", _ec);
            c = true;
            ec = _ec;
        }
        uint32_t on_message(uintptr_t& u, uintptr_t _msg) noexcept override {
            fprintf(stdout, "%-10s %u\n", __func__, u);
            fprintf(stdout, "  thread   %u\n", get_current_thread_id());
            fprintf(stdout, "  message  %zu\n", msg);
            u = (uintptr_t)get_current_thread_id();
            msg = _msg;
            return 0;
        }
    };

    impl_t impl{};
    service_thread_t proxy{impl};

    SECTION("check invocation") {
        const auto timeout = chrono::milliseconds{10};
        this_thread::sleep_for(timeout);
        // on_begin: the service is running
        REQUIRE(impl.b);
        // on_message
        REQUIRE(proxy.send(0xAA) == 0);
        this_thread::sleep_for(timeout);
        CAPTURE(impl.msg == 0xAA);
        // on_close, on_end
        REQUIRE(proxy.join(timeout) == 0);
        REQUIRE(impl.c);
        REQUIRE(impl.ec == 0);
    }
    SECTION("join: short timeout") {
        const auto timeout = chrono::milliseconds{};
        REQUIRE(proxy.join(timeout) == EINPROGRESS);
    }
    SECTION("join: multiple times") {
        REQUIRE(proxy.join(chrono::milliseconds{30}) == 0);
        // if already joined, it is not recoverable
        REQUIRE(proxy.join(chrono::milliseconds{0}) ==
                static_cast<uint32_t>(errc::state_not_recoverable));
    }
}

TEST_CASE("echo back") {
    struct impl_t final : public service_callback_t {
        message_queue_t mq;

      private:
        uint32_t on_begin(uintptr_t& u) noexcept override {
            return 0;
        }
        void on_end(uintptr_t u) noexcept override {
            mq.widow();
        }
        void on_close(uintptr_t u, uint32_t ec) noexcept override {
        }
        uint32_t on_message(uintptr_t& u, uintptr_t msg) noexcept override {

            if (msg == 0)
                return EINVAL;
            return mq.send(msg); // echo the message
        }
    };

    impl_t impl{};
    service_thread_t proxy{impl};

    auto timeout = chrono::milliseconds{10};
    SECTION("send and join") {
        REQUIRE(proxy.send(0x1) == 0);
        {
            uintptr_t msg{};
            bool widowed = true;
            REQUIRE(impl.mq.recv(msg, widowed) == 0);
            REQUIRE_FALSE(widowed);
            REQUIRE(msg == 0x1);
        }
        REQUIRE(proxy.send(0x2) == 0);
        {
            uintptr_t msg{};
            bool widowed = true;
            REQUIRE(impl.mq.recv(msg, widowed) == 0);
            REQUIRE_FALSE(widowed);
            REQUIRE(msg == 0x2);
        }
        REQUIRE(proxy.join(timeout) == 0);
    }
    SECTION("error from 'on_message'") {
        REQUIRE(proxy.send(0x0) == 0);
        {
            // by sending invalid message, the thread will be closed
            // and it will widow the queue
            uintptr_t msg{};
            bool widowed = false;
            REQUIRE(impl.mq.recv(msg, widowed) == 0);
            REQUIRE(widowed);
        }
        // this is return from the thread
        REQUIRE(proxy.join(timeout) == EINVAL);
    }
}

TEST_CASE("echo notify") {
    struct impl_t final : public service_callback_t {
        notification_t ev;

      private:
        uint32_t on_begin(uintptr_t&) noexcept override {
            return 0;
        }
        void on_end(uintptr_t u) noexcept override {
            ev.signal(); // signal for its end
        }
        void on_close(uintptr_t u, uint32_t ec) noexcept override {
        }
        uint32_t on_message(uintptr_t& u, uintptr_t msg) noexcept override {
            if (msg == 0)
                return EINVAL;
            return ev.signal(); // signal for the message
        }
    };

    impl_t impl{};
    service_thread_t proxy{impl};

    const auto timeout = chrono::milliseconds{10};
    SECTION("send and wait") {
        REQUIRE(proxy.send(0x1) == 0);
        REQUIRE(impl.ev.wait() == 0); // from `on_message`
        REQUIRE(proxy.send(0x2) == 0);
        REQUIRE(impl.ev.wait() == 0);
        REQUIRE(proxy.join(timeout) == 0);
    }
    SECTION("join and wait") {
        REQUIRE(proxy.send(0x0) == 0);
        REQUIRE(impl.ev.wait() == 0); // from `on_end`
        REQUIRE(proxy.join(timeout) == EINVAL);
    }
}
