#pragma once
#include <stop_token>
#include <string>
#include <cstddef>

namespace partyboard::net {
inline constexpr size_t NativeRoomHttpResponseLimit=1024*1024+4096;
struct NativeRoomHttpRequest {
    std::string url,method,bearer,body;
    unsigned timeoutMs=2500;
    // The mailbox bounds count events; allow the JSON response envelope too.
    size_t maximumBytes=NativeRoomHttpResponseLimit;
};
struct NativeRoomHttpResponse {unsigned status=0;std::string body;};
// Executed only by the room worker. The Windows implementation uses async
// WinHTTP internally so Stop can cancel a pending request without closing a
// synchronous handle from another thread.
NativeRoomHttpResponse nativeRoomHttp(const NativeRoomHttpRequest&,std::stop_token);
}
