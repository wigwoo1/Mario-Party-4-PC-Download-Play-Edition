#include "../extern/aurora/lib/gfx/render_worker.hpp"
#include <cassert>
#include <cstdio>
#include <emscripten.h>
namespace rw=aurora::gfx::render_worker;
int main() {
    for(int cycle=0;cycle<3;++cycle) {
        rw::initialize(); rw::initialize();
        assert(rw::is_worker_thread());
        int phase=0;
        for(int frame=0;frame<100;++frame) {
            rw::enqueue_begin_frame(frame,[&]{assert(phase==0);phase=1;});
            rw::enqueue_encode_pass(frame,0,[&]{
                assert(phase==1);phase=2;
                rw::enqueue_work([&]{assert(phase==2);phase=3;});
                rw::synchronize();
            });
            rw::enqueue_end_frame(frame,[&]{assert(phase==3);phase=0;});
            rw::synchronize(); assert(phase==0 && rw::is_idle());
        }
        rw::FrameSlotPool slots(2);
        auto first=slots.try_acquire(),second=slots.try_acquire();
        assert(first && second && first!=second && !slots.try_acquire());
        slots.release(*first); assert(slots.acquire()==*first);
        slots.reset(); assert(slots.free_count()==2);
        rw::shutdown(); rw::shutdown(); assert(!rw::is_worker_thread());
    }
    rw::FrameSlotPool delayed(1);
    assert(delayed.acquire()==0);
    emscripten_async_call([](void *context) {
        static_cast<rw::FrameSlotPool*>(context)->release(0);
    },&delayed,10);
    assert(delayed.acquire()==0);
    delayed.release(0);
    assert(delayed.free_count()==1);
    puts("Render worker passed: 300 inline frames, nested work, synchronization, slots and restart.");
}
