#include "../include/port/controller_routes.hpp"
#include "../include/port/lobby_start.hpp"
#include <cassert>
#include <cstdio>
using namespace partyboard::net;
int main() {
    Compatibility config;
    config.build[0] = config.assets[0] = config.save[0] = config.settings[0] = 1;
    ReadyGate lobby(11, config);
    assert(lobby.assign(11, 0, 11) && lobby.assign(11, 1, 22));
    ControllerCapabilities gate(lobby.owners(), lobby.revision());
    assert(lobby.ready(11, lobby.revision(), config, true));
    assert(lobby.ready(22, lobby.revision(), config, true));
    assert(!startLobby(lobby, gate, 11, lobby.revision()));
    assert(gate.offer(11, gate.revision(), 0) && gate.offer(22, gate.revision(), 0));
    assert(gate.acknowledge(11, gate.revision(), 0));
    assert(!startLobby(lobby, gate, 11, lobby.revision()));
    assert(gate.acknowledge(22, gate.revision(), 0));
    assert(!startLobby(lobby, gate, 22, lobby.revision()));
    config.seed = 123;
    assert(lobby.configure(11, config));
    assert(!startLobby(lobby, gate, 11, lobby.revision())); // Old capability revision.
    ControllerCapabilities refreshed(lobby.owners(), lobby.revision());
    assert(refreshed.offer(11, refreshed.revision(), 0) && refreshed.offer(22, refreshed.revision(), 0));
    assert(refreshed.acknowledge(11, refreshed.revision(), 0));
    assert(refreshed.acknowledge(22, refreshed.revision(), 0));
    assert(!startLobby(lobby, refreshed, 11, lobby.revision())); // Compatibility readiness cleared.
    assert(lobby.ready(11, lobby.revision(), config, true) && lobby.ready(22, lobby.revision(), config, true));
    assert(startLobby(lobby, refreshed, 11, lobby.revision()));
    assert(!startLobby(lobby, refreshed, 11, lobby.revision()));
    const std::array<uint32_t, 4> owners{11, 22, 11, 22};
    ControllerRoutes host(owners, 11, {1, -1, 0, -1});
    ControllerRoutes guest(owners, 22, {-1, 0, -1, 1});
    ControllerCapabilities capabilities(owners, 7);
    CapabilityOffer offer{SessionId{9}, 7, 0xa0000000};
    const auto wire = encodeCapabilities(offer);
    assert(decodeCapabilities(wire) == offer);
    for (size_t size = 0; size < wire.size(); ++size)
        assert(!decodeCapabilities(std::span(wire).first(size)));
    std::array<uint8_t, 37> oversized{};
    for (unsigned i = 0; i < wire.size(); ++i) oversized[i] = wire[i];
    assert(!decodeCapabilities(oversized));
    for (unsigned offset : {0u, 4u, 5u, 6u, 7u, 32u}) {
        auto bad = wire; bad[offset] ^= 2;
        assert(!decodeCapabilities(bad));
    }
    ControllerCapabilities received(owners, 7);
    assert(!received.receive(11, SessionId{8}, wire));
    assert(!received.receive(22, offer.session, wire));
    assert(!received.agreedMask());
    assert(received.receive(11, offer.session, wire));
    assert(received.receive(22, offer.session, encodeCapabilities({offer.session, 7, 0})));
    assert(received.agreedMask() == offer.mask);
    const auto ackWire = encodeCapabilityAck(offer);
    assert(decodeCapabilityAck(ackWire) == offer);
    assert(!decodeCapabilityAck(wire) && !decodeCapabilities(ackWire));
    for (size_t size = 0; size < ackWire.size(); ++size)
        assert(!decodeCapabilityAck(std::span(ackWire).first(size)));
    auto badAck = ackWire; badAck[5] = 1;
    assert(!decodeCapabilityAck(badAck));
    assert(!received.receiveAck(11, offer.session, wire));
    assert(!received.receiveAck(11, SessionId{8}, ackWire));
    assert(!received.receiveAck(33, offer.session, ackWire));
    assert(!received.ready());
    assert(received.receiveAck(11, offer.session, ackWire));
    assert(!received.ready());
    assert(received.receiveAck(22, offer.session, ackWire));
    assert(received.ready());
    assert(!capabilities.agreedMask());
    assert(!capabilities.acknowledge(11, 7, 0));
    assert(!capabilities.ready());
    assert(!capabilities.offer(11, 6, 0xa0000000));
    assert(!capabilities.offer(33, 7, 0));
    assert(!capabilities.offer(11, 7, 0x40000000));
    assert(!capabilities.offer(11, 7, 1));
    assert(capabilities.offer(11, 7, 0xa0000000));
    assert(!capabilities.agreedMask());
    assert(capabilities.offer(11, 7, 0xa0000000));
    assert(!capabilities.offer(11, 7, 0));
    assert(capabilities.offer(22, 7, 0)); // Explicitly no rumble is still ready.
    assert(capabilities.agreedMask() == 0xa0000000);
    assert(!capabilities.ready());
    assert(!capabilities.acknowledge(33, 7, 0xa0000000));
    assert(!capabilities.acknowledge(11, 6, 0xa0000000));
    assert(!capabilities.acknowledge(11, 7, 0));
    assert(capabilities.acknowledge(11, 7, 0xa0000000));
    assert(!capabilities.ready());
    assert(capabilities.acknowledge(22, 7, 0xa0000000));
    assert(capabilities.ready());
    assert(!capabilities.offer(22, 7, 0x40000000)); // Frozen contribution.
    assert(capabilities.agreedMask() == 0xa0000000);
    ControllerCapabilities inactive({11, 0, 0, 0}, 8);
    assert(!inactive.offer(11, 8, 0x40000000));
    assert(inactive.offer(11, 8, 0));
    assert(inactive.agreedMask().has_value() && *inactive.agreedMask() == 0);
    assert(inactive.acknowledge(11, 8, 0) && inactive.ready());
    inactive.cancel();
    assert(!inactive.ready() && !inactive.agreedMask());
    assert(!inactive.acknowledge(11, 8, 0));
    assert(!inactive.offer(11, 8, 0));
    PADStatus physical[4]{};
    physical[0].button = 0x100; physical[0].stickX = -127; physical[0].extButton = 55;
    physical[1].button = 0x200; physical[1].triggerLeft = 230;
    auto h = host.route(physical), g = guest.route(physical);
    assert(h[0].button == 0x200 && h[2].button == 0x100);
    assert(g[1].button == 0x100 && g[3].button == 0x200);
    assert(g[1].stickX == -127 && g[3].triggerLeft == 230 && g[1].extButton == 0);
    assert(g[0].err == PAD_ERR_NO_CONTROLLER && g[0].button == 0);
    assert(host.rumbleContribution(0x80000000) == 0x20000000);
    assert(guest.rumbleContribution(0x80000000) == 0x40000000);
    assert(guest.rumbleContribution(0x40000000) == 0x10000000);
    assert(host.motorDevice(0) == 1 && host.motorDevice(2) == 0);
    assert(guest.motorDevice(1) == 0 && guest.motorDevice(3) == 1);
    assert(guest.motorDevice(0) == -1 && guest.motorDevice(2) == -1);
    assert(guest.motorDevice(4) == -1 && guest.motorDevice(~0u) == -1);
    ControllerRoutes single({11, 0, 0, 0}, 11, {2, -1, -1, -1});
    assert(single.motorDevice(0) == 2 && single.motorDevice(1) == -1);
    assert(single.rumbleContribution(0x20000000) == 0x80000000);
    assert(single.rumbleContribution(0xc0000000) == 0);
    physical[0].err = PAD_ERR_NO_CONTROLLER;
    assert(guest.route(physical)[1].err == PAD_ERR_NO_CONTROLLER);
    // Routing feeds the real exchange: local controller zero can own game slot two.
    PadExchange a(SessionId{1}, owners, 11), b(SessionId{1}, owners, 22);
    assert(a.begin(h.data(), 42) && b.begin(g.data(), 42));
    for (const auto& packet : a.outgoing()) assert(b.receive(11, packet) == Receive::Accepted);
    for (const auto& packet : b.outgoing()) assert(a.receive(22, packet) == Receive::Accepted);
    assert(a.poll(42, h.data()) == Exchange::Ready && b.poll(42, g.data()) == Exchange::Ready);
    for (unsigned slot = 0; slot < 4; ++slot) assert(packPad(h[slot]) == packPad(g[slot]));
    assert(g[0].button == 0x200 && g[1].button == 0x100);
    for (auto bad : {std::array<int, 4>{0, 1, 2, -1}, {0, -1, 0, -1},
                     {-1, -1, 0, -1}, {4, -1, 0, -1}, {-2, -1, 0, -1}}) {
        bool rejected = false;
        try { ControllerRoutes invalid(owners, 11, bad); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    ControllerRoutes spectator(owners, 33, {-1, -1, -1, -1});
    assert(spectator.rumbleContribution(0xf0000000) == 0);
    puts("Controller routing tests passed: ownership, permutation, merged input and rumble contribution");
}
