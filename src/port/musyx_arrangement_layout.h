#ifndef PARTYBOARD_MUSYX_ARRANGEMENT_LAYOUT_H
#define PARTYBOARD_MUSYX_ARRANGEMENT_LAYOUT_H
#include <stdint.h>
#include <stddef.h>
/* Each byte records its source displacement within an endian field. This
 * makes repeated references idempotent and rejects incompatible overlapping
 * fields before the owned sequence copy is changed. 1 means raw byte. */
static inline int PartyboardMusyxLayoutMark(uint8_t *layout, size_t bytes,
        size_t offset, size_t count, unsigned width) {
    size_t i;
    if(!layout || (width!=1 && width!=2 && width!=4)
            || count%width || offset>bytes || count>bytes-offset) return 0;
    for(i=0;i<count;++i) {
        uint8_t kind=(uint8_t)(width==1 ? 1 : width==2 ? 2+i%2 : 4+i%4);
        if(layout[offset+i] && layout[offset+i]!=kind) return 0;
    }
    for(i=0;i<count;++i)
        layout[offset+i]=(uint8_t)(width==1 ? 1 : width==2 ? 2+i%2 : 4+i%4);
    return 1;
}
static inline int PartyboardMusyxLayoutApply(uint8_t *destination,
        const uint8_t *source, const uint8_t *layout, size_t bytes) {
    size_t i;
    const uint16_t endian=1;
    int little=*(const uint8_t*)&endian;
    if(!destination || !source || !layout || destination==source) return 0;
    /* Validate all field boundaries before writing any destination byte. */
    for(i=0;i<bytes;) {
        unsigned width=layout[i]==2 ? 2 : layout[i]==4 ? 4 : 1, j;
        if(layout[i]>1 && width==1) return 0;
        if(width>bytes-i) return 0;
        for(j=1;j<width;++j) if(layout[i+j]!=layout[i]+j) return 0;
        i+=width;
    }
    for(i=0;i<bytes;) {
        unsigned width=layout[i]==2 ? 2 : layout[i]==4 ? 4 : 1, j;
        for(j=0;j<width;++j) destination[i+j]=source[i+(little ? width-1-j : j)];
        i+=width;
    }
    return 1;
}
#endif
