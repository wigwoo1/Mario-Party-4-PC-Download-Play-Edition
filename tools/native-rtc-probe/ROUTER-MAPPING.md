Production native Host sessions now own the router worker. Initial invitations
and authenticated host replacement offers retain mapping tickets and publish
verified mapped candidates in copied SDP. The current local game test exercised
ICE fallback, so actual router forwarding and Internet acceptance remain unproven.

Dependency: MiniUPnPc 2.3.3, miniupnp commit
`bf4215a7574f88aa55859db9db00e3ae58cf42d6` (tag `miniupnpc_2_3_3`). Obtain from
https://github.com/miniupnp/miniupnp.git into a development-only directory outside
the package. CMake verifies this exact HEAD and unmodified tracked miniupnpc tree.
The upstream BSD 3-Clause LICENSE is retained in `licenses/miniupnpc-LICENSE`
and copied into the local package. This does not replace the broader dependency
notice audit.

Configure this existing probe project with `PARTYBOARD_TEST_ROUTER_MAPPING=ON`
and `PARTYBOARD_MINIUPNPC_SOURCE` naming that checkout, in addition to its normal
RTC, MbedTLS and JSON settings. On this machine use
`CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`, consistent with the native build.
Build targets `partyboard_router_mapping_test`,
`partyboard_router_mapping_http_probe`, `partyboard_router_candidates_test`, and
`partyboard_router_worker_test`. Run the policy, candidate and worker executables
directly. From the workspace root run
`node outputs/partyboard/tools/test-native-router-http.cjs` with the second
executable path as the first optional argument and report path as the second.
These tests do not call SSDP discovery or contact a physical router.

The controller uses a fresh OS-random ownership token, a finite 600-second lease,
and renewal at half the confirmed remaining duration. Existing mappings are
conflicts, including mappings with the same internal address/port or application
name. Renewal and removal first query exact address/port/token ownership. Only
UPnP error 714 establishes absence; HTTP errors and malformed replies do not.
Every add/delete is followed by a readback, including a lost HTTP response.
Permanent or missing/invalid lease durations are rejected and owned cleanup is
attempted. Unknown cleanup is retained as pending and may be retried. The finite
lease is a fallback when the process/router becomes unavailable. Routers which
ignore lease semantics cannot be certified by these fixture tests.

`nativeRouterPortRequests` selects only actual gathered IPv4 UDP host candidates
on the discovered gateway's LAN interface and deduplicates their bound ports.
It neither allocates nor guesses a port. A real libdatachannel gathering test
verifies those ports; separate injected candidates exercise filtering.

`NativeRouterWorker` now runs discovery and mapping operations off the caller's
thread. Tickets share a lease for the same address/port; dropping an original
ticket keeps that lease while a replacement ticket still needs it. Changed ports
get separate leases. Closed tickets immediately hide their mapped endpoints;
snapshots also discard expired leases. No-gateway discovery completes with empty
mapping results, preserving the caller's ability to use normal ICE.

On Windows, CMake replaces the upstream `connecthostport.c` and `receivedata.c`
objects with `native_router_io_win.cpp`, without modifying the pinned checkout.
Numeric IPv4 connects use nonblocking sockets with deadline/cancellation checks;
receive waits check cancellation in slices of at most 50 ms. Per-operation total
budgets are 3 seconds and 256 KiB, with a separate 5-second/1-MiB discovery budget.
Nested budgets cannot extend an outer cleanup deadline. Hostname URLs fail closed
without a DNS lookup. The POSIX timeout macro is not forced on Windows. This hook
target requires Windows 7 API definitions; the game retains its existing runtime
platform requirements. Other platforms' default worker factory returns no router
until equivalently bounded socket hooks exist.

Closing the worker cancels current I/O, closes tickets, then attempts owned
cleanup under a separate total 3-second budget. A failed readback returns false
from `close()`; it is not reported as successful deletion. Finite leases remain
the fallback for unavailable routers. Twenty loopback HTTP scenarios exercise
the prior ownership cases plus stalled/trickled/oversized replies, cancellation,
an outer deadline, hostname refusal, shared tickets and unresponsive cleanup.
These tests establish measured local deadline behavior, not physical gateway
timing or successful cleanup when the gateway cannot be contacted.

`NativeRouterConnectionMapping` starts a ticket only after actual ICE gathering.
It waits at most 12 seconds, freezes one published SDP, retains valid leases for
that route, and closes late or unusable mapping requests. The copy gains only
public IPv4 server-reflexive candidates for exact gathered address/port pairs;
original ICE credentials, fingerprint and local description are preserved. Host
replacement offers sign the resulting description using the existing identity.
Once authenticated input switches to a replacement, the original mapping ticket
retires. Both original and replacement owners close their tickets on teardown.

Native RTC Windows builds default `PARTYBOARD_NATIVE_UPNP=ON` and require the pinned
source path. The installed build and 314-file local candidate include this code
and its license. Focused tests decode mapped offers without sending packets to
their fake public addresses. An actual native Host/three-Chrome test passed full
asset transfers, preparation, 60 matching input records and teardown with the
worker enabled; all three routes used ICE fallback. See
`outputs/validation/production-native-upnp-20261016.independent-audit.json` under
the workspace root. Real gateway forwarding/lease cleanup, WAN/TURN and latest
build complete-match/reconnect acceptance still need verification.

Windows API references: [select](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select)
and [setsockopt](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt).

UPnP v1 exposes no atomic ownership-conditioned add/delete. The controller checks
immediately before each mutation and refuses known foreign entries, but another
control point can race the query and write. That protocol limitation remains a
real-router/ownership acceptance concern; the fixture is not proof against
simultaneous external changes between those operations.
