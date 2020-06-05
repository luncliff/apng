#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>

#include <service.hpp>

TEST_CASE("async service") {
    SECTION("spawn 1 by 1") {
        auto tester = [](async_service_t& service,
                         notification_t& n) -> fire_and_forget {
            co_await service;
            if (int ec = n.signal())
                throw std::system_error{ec, std::system_category()};
        };
        notification_t ev{};
        async_service_t service{};

        tester(service, ev);
        REQUIRE(ev.wait() == 0);
        tester(service, ev);
        REQUIRE(ev.wait() == 0);
    }
    SECTION("spawn sequence") {
        auto tester = [](async_service_t& service, message_queue_t& mq,
                         uint32_t id) -> fire_and_forget {
            co_await service;
            switch (id) {
            case 4:
                if (int ec = mq.widow())
                    throw std::system_error{ec, std::system_category()};
                co_return;
            default:
                if (int ec = mq.send(id))
                    throw std::system_error{ec, std::system_category()};
            }
        };
        message_queue_t mq{};
        async_service_t service{};
        tester(service, mq, 1);
        tester(service, mq, 2);
        tester(service, mq, 3);
        tester(service, mq, 4);

        uintptr_t msg = 0;
        bool widowed = false;
        REQUIRE(mq.recv(msg, widowed) == 0);
        REQUIRE(msg == 1);
        REQUIRE(mq.recv(msg, widowed) == 0);
        REQUIRE(msg == 2);
        REQUIRE(mq.recv(msg, widowed) == 0);
        REQUIRE(msg == 3);
        REQUIRE(mq.recv(msg, widowed) == 0);
        REQUIRE(widowed);
    }
}
