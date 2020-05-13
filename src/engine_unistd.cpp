#include "thread_proxy.hpp"

uint64_t get_current_thread_id() noexcept {
  // thre result can be either `uintptr_t` or `void*`
  return (uint64_t)pthread_self();
}

message_queue_t::message_queue_t() noexcept(false) {
  if (pipe(fds) != 0)
    throw std::system_error{errno, std::system_category(), "pipe"};
}
message_queue_t::~message_queue_t() noexcept {
  close(fds[1]); // widow the reader end
  close(fds[0]);
}

uint32_t message_queue_t::send(uintptr_t user_data) noexcept {
  if (write(fds[1], &user_data, sizeof(uintptr_t)) < 0)
    return errno;
  return 0;
}
uint32_t message_queue_t::recv(uintptr_t &user_data, bool &widowed) noexcept {
  const auto sz = read(fds[0], &user_data, sizeof(uintptr_t));
  widowed = (sz == 0); // EOF
  if (sz < 0)
    return errno;
  return 0;
}
uint32_t message_queue_t::widow() noexcept {
  if (close(fds[1])) // widow the reader end
    return errno;
  return 0;
}

notification_t::notification_t() noexcept(false) {
  if (pipe(fds) != 0)
    throw std::system_error{errno, std::system_category(), "pipe"};
}
notification_t::~notification_t() noexcept {
  close(fds[1]);
  close(fds[0]);
}
uint32_t notification_t::signal() noexcept {
  pthread_t sender = pthread_self();
  if (write(fds[1], &sender, sizeof(pthread_t)) < 0)
    return errno;
  return 0;
}

uint32_t notification_t::wait() noexcept {
  pthread_t sender{};
  const auto sz = read(fds[0], &sender, sizeof(pthread_t));
  if (sz < 0)
    return errno;
  return 0;
}
