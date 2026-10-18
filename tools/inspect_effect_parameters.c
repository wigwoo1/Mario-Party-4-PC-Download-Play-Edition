#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/musyx_effect_parameters.h"
int main(void) {
    uint8_t header[4],*raw;
    size_t bytes,i;
    PartyboardMsmContainer container;
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    if(fread(header,1,4,stdin)!=4) return 2;
    bytes=PartyboardMsmU32(header);
    if(bytes<96 || bytes>67108864) return 2;
    raw=malloc(bytes);if(!raw) return 3;
    if(fread(raw,1,bytes,stdin)!=bytes || !PartyboardMsmContainerOpen(&container,raw,bytes)) return 4;
    for(i=0;i<container.section[PARTYBOARD_MSM_EFFECTS].bytes/16;++i) {
        PartyboardMsmContainerEffect effect;
        SND_PARAMETER parameters[5];
        unsigned j;
        int valid;
        if(!PartyboardMsmContainerGetEffect(&effect,&container,i)) return 5;
        valid=PartyboardMusyxEffectParameters(parameters,&effect,127,64,0,effect.reverb,effect.chorus);
        printf("%d",valid);
        if(valid) for(j=0;j<5;++j) printf(" %u %u",parameters[j].ctrl,
            (j==2 || j==3)?parameters[j].paraData.value7:parameters[j].paraData.value14);
        puts("");
    }
    free(raw);return 0;
}
