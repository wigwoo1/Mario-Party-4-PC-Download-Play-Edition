#include "libco.h"
#include <assert.h>
#include <stdio.h>

static cothread_t root, workers[3];
static unsigned starting, counts[3], finished[3];
static cothread_t ping, pong;
static unsigned phase;
static void ping_entry(void) {
    volatile unsigned marker = 0xabcdef01u;
    for (unsigned i=0;i<50;++i) {
        assert(co_active()==ping && phase==i*2);
        ++phase; co_switch(pong);
        assert(marker==0xabcdef01u && phase==i*2+2);
    }
    for (;;) co_switch(root);
}
static void pong_entry(void) {
    volatile unsigned marker = 0x10203040u;
    for (unsigned i=0;i<50;++i) {
        assert(co_active()==pong && phase==i*2+1 && marker==0x10203040u);
        ++phase; co_switch(ping);
    }
    for (;;) co_switch(root);
}
static void nested(unsigned id, unsigned depth) {
    volatile unsigned marker = 0x12340000u + id * 100u + depth;
    if (depth) nested(id, depth - 1);
    else for (unsigned round=0;round<100;++round) {
        assert(co_active() == workers[id]);
        ++counts[id];
        co_switch(root);
        assert(marker == 0x12340000u + id * 100u);
    }
    assert(marker == 0x12340000u + id * 100u + depth);
}
static void worker(void) {
    const unsigned id = starting;
    nested(id, 8);
    finished[id]=1;
    for (;;) co_switch(root); /* Do not return from a coroutine entry. */
}
int main(void) {
    root = co_active(); assert(root);
    for (unsigned i=0;i<3;++i) {workers[i]=co_create(65536,worker);assert(workers[i]);}
    for (unsigned round=0;round<=100;++round) for (unsigned i=0;i<3;++i) {
        starting=i;
        co_switch(workers[i]);
        assert(co_active()==root && counts[i]==(round<100?round+1:100));
        assert(finished[i]==(round==100));
    }
    for (unsigned i=0;i<3;++i) co_delete(workers[i]);
    for (unsigned cycle=0;cycle<20;++cycle) {
        phase=0;
        ping=co_create(65536,ping_entry); pong=co_create(131072,pong_entry);
        assert(ping && pong);
        co_switch(ping);
        assert(co_active()==root && phase==100);
        co_delete(pong); co_delete(ping);
    }
    puts("Coroutine contract passed: three stacks, 300 yields, preserved locals and root identity");
    puts("Coroutine lifecycle passed: 20 create/delete cycles and 2000 direct child switches");
    return 0;
}
