"""Extract unchanged original volume preparation block for direct parity tests."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',required=True)
p.add_argument('--header',required=True)
a=p.parse_args()
s=Path(a.source).read_text()
start=s.index('          if ((dsp_vptr->changed[0] & 1) != 0) {')
end=s.index('          if ((dsp_vptr->changed[0] & 0x200) != 0) {',start)
block=s[start:end]
startup_start=s.index('            pb->mix.vL = dsp_vptr->lastVolL = dsp_vptr->volL;')
startup_end=s.index('            dsp_vptr->state = 2;',startup_start)
startup=s[startup_start:startup_end]
position_start=s.index('static void sal_update_hostplayinfo(')
position_end=s.index('static void AddDpop(',position_start)
position=s[position_start:position_end]
Path(a.header).write_text('static void original_prepare_standard(DSPvoice *dsp_vptr, u16 rampResetOffsetFlags[5]) {\n'
    '  _PB *pb=dsp_vptr->pb;\n  u32 needsDelta=0;\n'
    '  struct { SND_STUDIO_TYPE type; } studio={SND_STUDIO_TYPE_STD}, *stp=&studio;\n'
    +block+'\n}\n'
    +'static void original_init_standard(DSPvoice *dsp_vptr) {\n'
    +'  _PB *pb=dsp_vptr->pb;\n'
    +'  struct { SND_STUDIO_TYPE type; } studio={SND_STUDIO_TYPE_STD}, *stp=&studio;\n'
    +startup+'\n}\n'+position)
print('Unchanged original standard volume preparation/startup blocks extracted')
