#ifndef PARTYBOARD_GLYPH_PANEL_PIXELS_H
#define PARTYBOARD_GLYPH_PANEL_PIXELS_H
/* Allocation-free GX texture decoding. Output is linear RGBA8.
 * Unsupported/truncated input fails before output is written. No overlapping buffers. */
#include <stddef.h>
#include <stdint.h>
static uint16_t GlyphPanelBE16(const uint8_t *p) { return ((uint16_t)p[0]<<8)|p[1]; }
static void GlyphPanelRGB5A3(uint16_t v, uint8_t *c) {
 if(v&32768){c[0]=((v>>10)&31)*255/31;c[1]=((v>>5)&31)*255/31;c[2]=(v&31)*255/31;c[3]=255;}
 else{c[0]=((v>>8)&15)*17;c[1]=((v>>4)&15)*17;c[2]=(v&15)*17;c[3]=((v>>12)&7)*255/7;}
}
static void GlyphPanelRGB565(uint16_t v,uint8_t *c){c[0]=((v>>11)&31)*255/31;c[1]=((v>>5)&63)*255/63;c[2]=(v&31)*255/31;c[3]=255;}
static int GlyphPanelDecode(unsigned format,unsigned w,unsigned h,const uint8_t *data,size_t size,const uint8_t *palette,size_t paletteSize,uint8_t *out,size_t outSize){
 size_t required,x,y,offset;unsigned k;
 if(!data||!out||!w||!h||w>512||h>512||outSize<(size_t)w*h*4)return 0;
 if(format==0)required=((w+3)/4)*((h+3)/4)*64;
 else if(format==3)required=((w+7)/8)*((h+3)/4)*32;
 else if(format==10)required=((w+7)/8)*((h+7)/8)*32;
 else return 0;
 if(size<required||(format==3&&(!palette||paletteSize<512)))return 0;
 for(y=0;y<h;y++)for(x=0;x<w;x++){
  uint8_t *c=out+(y*w+x)*4;
  if(format==0){offset=((y/4)*((w+3)/4)+x/4)*64+((y%4)*4+x%4)*2;c[0]=data[offset+1];c[1]=data[offset+32];c[2]=data[offset+33];c[3]=data[offset];}
  else if(format==3){offset=((y/4)*((w+7)/8)+x/8)*32+(y%4)*8+x%8;GlyphPanelRGB5A3(GlyphPanelBE16(palette+data[offset]*2),c);}
  else{
   uint8_t colors[4][4];uint16_t a,b;unsigned index;
   offset=((y/8)*((w+7)/8)+x/8)*32+((y%8)/4*2+(x%8)/4)*8;
   a=GlyphPanelBE16(data+offset);b=GlyphPanelBE16(data+offset+2);GlyphPanelRGB565(a,colors[0]);GlyphPanelRGB565(b,colors[1]);
   for(k=0;k<3;k++){colors[2][k]=a>b?(2*colors[0][k]+colors[1][k])/3:(colors[0][k]+colors[1][k])/2;colors[3][k]=a>b?(colors[0][k]+2*colors[1][k])/3:0;}
   colors[2][3]=255;colors[3][3]=a>b?255:0;index=(data[offset+4+y%4]>>(6-2*(x%4)))&3;for(k=0;k<4;k++)c[k]=colors[index][k];
  }
 }
 return 1;
}
/* Replace one authored button square using a blank panel column at the same
 * height. All bounds are checked first; pixels outside the square stay exact.
 * Glyph and panel buffers must not overlap. Inputs are straight-alpha RGBA. */
static int GlyphPanelStamp(uint8_t *panel, unsigned w, unsigned h, size_t size,
    unsigned x, unsigned y, unsigned side, unsigned blankX,
    const uint8_t *glyph, unsigned gw, unsigned gh, size_t glyphSize) {
    unsigned dx,dy,k;
    if (!panel || !glyph || !w || !h || w>512 || h>512 ||
        !gw || !gh || gw>512 || gh>512 || !side || side>512 ||
        size<(size_t)w*h*4 || glyphSize<(size_t)gw*gh*4 ||
        x>=w || y>=h || side>w-x || side>h-y || blankX>=w ||
        (blankX>=x && blankX<x+side)) return 0;
    for (dy=0;dy<side;dy++) for (dx=0;dx<side;dx++) {
        uint8_t *dst=panel+((y+dy)*w+x+dx)*4;
        const uint8_t *bg=panel+((y+dy)*w+blankX)*4;
        const uint8_t *fg=glyph+(((size_t)dy*gh/side)*gw+(size_t)dx*gw/side)*4;
        unsigned alpha=fg[3]*255u+bg[3]*(255u-fg[3]);
        for(k=0;k<3;k++) dst[k]=alpha ?
            (fg[k]*fg[3]*255u+bg[k]*bg[3]*(255u-fg[3])+alpha/2)/alpha : 0;
        dst[3]=(alpha+127)/255;
    }
    return 1;
}
static int GlyphPanelEncodeRGBA8(const uint8_t *rgba,unsigned w,unsigned h,
    size_t rgbaSize,uint8_t *out,size_t outSize) {
    size_t need,x,y,offset,i;
    if(!rgba||!out||!w||!h||w>512||h>512||rgbaSize<(size_t)w*h*4)return 0;
    need=((w+3)/4)*((h+3)/4)*64;
    if(outSize<need)return 0;
    for(i=0;i<need;i++)out[i]=0;
    for(y=0;y<h;y++)for(x=0;x<w;x++){
        const uint8_t *c=rgba+(y*w+x)*4;
        offset=((y/4)*((w+3)/4)+x/4)*64+((y%4)*4+x%4)*2;
        out[offset]=c[3];out[offset+1]=c[0];out[offset+32]=c[1];out[offset+33]=c[2];
    }
    return 1;
}
#endif
