#include "../src/port/musyx_depop_voice.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    for(unsigned mask=0;mask<=65535;++mask) {
        _PB pb={0};DSPvoice voice={0};DSPhostDPop sum={0};
        voice.pb=&pb;voice.postBreak=1;pb.state=1;pb.mixerCtrl=(u16)mask;
        pb.dpop.aL=1;pb.dpop.aR=65534;pb.dpop.aS=3;
        pb.dpop.aAuxAL=4;pb.dpop.aAuxAR=65531;pb.dpop.aAuxAS=6;
        pb.dpop.aAuxBL=7;pb.dpop.aAuxBR=65528;pb.dpop.aAuxBS=9;
        assert(PartyboardMusyxDepopVoice200(&sum,&voice));
        assert(!pb.state && !voice.postBreak);
        assert(sum.l==1 && sum.r==-2);
        assert(sum.s==((mask&4)?3:0));
        assert(sum.lA==((mask&1)?4:0) && sum.rA==((mask&1)?-5:0));
        assert(sum.sA==((mask&1)&&(mask&0x14)?6:0));
        assert(sum.lB==((mask&0x12)?7:0) && sum.rB==((mask&0x12)?-8:0));
        assert(sum.sB==((mask&0x12)&&(mask&4)?9:0));
    }
    assert(PartyboardMusyxDepopSigned(32768)==-32768);
    assert(PartyboardMusyxDepopSigned(65535)==-1);
    {
        DSPhostDPop sum={-0x7fffff,0x7fffff,-3200,3200,-160,160,-159,159,0};
        _SPB pb={0};
        assert(PartyboardMusyxDepopStudio200(&sum,&pb));
        assert(pb.dpopLHi==0xff80 && pb.dpopLLo==1 && pb.dpopLDelta==20);
        assert(pb.dpopRHi==0x7f && pb.dpopRLo==65535 && pb.dpopRDelta==65516);
        assert(pb.dpopSHi==65535 && pb.dpopSLo==62336 && pb.dpopSDelta==20);
        assert(pb.dpopALHi==0 && pb.dpopALLo==3200 && pb.dpopALDelta==65516);
        assert(pb.dpopARHi==65535 && pb.dpopARLo==65376 && pb.dpopARDelta==1);
        assert(pb.dpopASHi==0 && pb.dpopASLo==160 && pb.dpopASDelta==65535);
        assert(pb.dpopBLHi==65535 && pb.dpopBLLo==65377 && !pb.dpopBLDelta);
        assert(pb.dpopBRHi==0 && pb.dpopBRLo==159 && !pb.dpopBRDelta);
        assert(!pb.dpopBSHi && !pb.dpopBSLo && !pb.dpopBSDelta);
        assert(sum.l==-0x7fffff+3200 && sum.r==0x7fffff-3200);
        assert(!sum.s && !sum.lA && !sum.rA && !sum.sA);
        assert(sum.lB==-159 && sum.rB==159 && !sum.sB);
        sum.sB=0x800000;
        { DSPhostDPop saved=sum;_SPB saved_pb=pb;
          assert(!PartyboardMusyxDepopStudio200(&sum,&pb));
          assert(!memcmp(&sum,&saved,sizeof sum) && !memcmp(&pb,&saved_pb,sizeof pb)); }
    }
    puts("Original MusyX 2.0 depop routing: all 65536 mixer controls PASS");
    return 0;
}
