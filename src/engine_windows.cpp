#include "engine.hpp"

#include <concrt.h> // Windows Concurrency Runtime
#include <system_error>

using namespace std;

static_assert(sizeof(uint32_t) == sizeof(decltype(GetLastError())));
static_assert(sizeof(ULONG_PTR) == sizeof(void *));

uint64_t get_current_thread_id() noexcept { return GetCurrentThreadId(); }

message_queue_t::message_queue_t() noexcept(false)
    : cp{CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, ULONG_PTR{}, 3)} {
  if (cp == NULL)
    throw std::system_error{static_cast<int>(GetLastError()),
                            std::system_category(), "CreateIoCompletionPort"};
}
message_queue_t::~message_queue_t() noexcept { CloseHandle(cp); }

uint32_t message_queue_t::send(uintptr_t user_data) noexcept { //
  if (PostQueuedCompletionStatus(cp, 0, GetCurrentThreadId(),
                                 reinterpret_cast<LPOVERLAPPED>(user_data)))
    return S_OK;
  return GetLastError();
}
uint32_t message_queue_t::recv(uintptr_t &user_data, bool &widowed) noexcept {
  DWORD timeout = INFINITE, size = 0;
  ULONG_PTR sender = 0; // DWORD sender thread id
  if (GetQueuedCompletionStatus(cp, &size, &sender,
                                reinterpret_cast<LPOVERLAPPED *>(&user_data),
                                timeout) == FALSE) {
    widowed = true;
    return GetLastError();
  }
  widowed = (sender == ERROR_INVALID_THREAD_ID);
  return S_OK;
}
uint32_t message_queue_t::widow() noexcept {
  if (PostQueuedCompletionStatus(cp, 0, ERROR_INVALID_THREAD_ID, NULL))
    return S_OK;
  return GetLastError();
}

notification_t::notification_t() noexcept(false)
    : handle{CreateEventW(nullptr, FALSE, FALSE, nullptr)} {
  if (handle == NULL)
    throw std::system_error{static_cast<int>(GetLastError()),
                            std::system_category(), "CreateEventW"};
}
notification_t::~notification_t() noexcept { CloseHandle(handle); }

uint32_t notification_t::wait() noexcept {
  switch (WaitForSingleObjectEx(handle, INFINITE, true)) {
  case WAIT_OBJECT_0:
    return S_OK;
  case WAIT_ABANDONED:
  case WAIT_IO_COMPLETION:
  case WAIT_FAILED:
  default:
    return GetLastError();
  }
}

uint32_t notification_t::signal() noexcept {
  if (SetEvent(handle) == FALSE)
    return GetLastError();
  return S_OK;
}

// DirectX 11
/*
#include <wrl/client.h>
#include <d3d11.h>
#include <d3d11_1.h>
// #include <d3dcompiler.h>
// #include <directxmath.h>
// #include <DirectXColors.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32")

struct dx11_context_t {
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    D3D_FEATURE_LEVEL level;

  public:
    dx11_context_t() noexcept(false) : device{}, context{}, level{} {
        if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0,
                                        nullptr, 0, D3D11_SDK_VERSION, &device,
                                        &level, &context);
            FAILED(hr))
            throw std::runtime_error{"D3D11CreateDevice"};
    }
    ~dx11_context_t() noexcept {
        if (device)
            device->Release();
        if (context)
            context->Release();
    }
};
*/