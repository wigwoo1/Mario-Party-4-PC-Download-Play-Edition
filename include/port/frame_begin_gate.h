#ifndef PARTYBOARD_FRAME_BEGIN_GATE_H
#define PARTYBOARD_FRAME_BEGIN_GATE_H

/* Call after accepting this frame's online input. Keep that input while the
 * presentation surface is unavailable: no simulation or second input poll may
 * run until a render frame exists. Pump events again after an asynchronous peer
 * wait so a newly visible canvas does not retain SDL's old hidden state. */
static inline int PartyBoardWaitForRenderFrame(int (*running)(void), int (*pump)(void),
                                              int (*begin)(void), void (*wait)(void))
{
    while (running()) {
        if (!pump()) return 0;
        if (begin()) return 1;
        wait();
    }
    return 0;
}

#endif
