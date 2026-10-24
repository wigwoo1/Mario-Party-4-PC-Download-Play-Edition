/* Registered compound menu panels retain their authored animation until kill. */
static void ControlPanelRefresh(s16 sprite) {
    ControlPanelSprite *state=&controlPanelSprites[sprite];
    ANIMDATA *original=state->original, *made=NULL, *glyph;
    ANIMBMP *bmp=original->bmp;
    u8 *pixels, glyphPixels[64*64*4];
    s16 ys[2], entries[2], count, n;
    size_t linearSize=(size_t)bmp->sizeX*bmp->sizeY*4;
    size_t tiledSize=((bmp->sizeX+3)/4)*((bmp->sizeY+3)/4)*64;
    BOOL changed=FALSE;
    if (++state->refresh<120) return;
    state->refresh=0;
    pixels=HuMemDirectMalloc(HEAP_DATA,linearSize);
    if (!pixels) return;
    if (!GlyphPanelDecode(bmp->dataFmt, bmp->sizeX,bmp->sizeY,bmp->data,bmp->dataSize,
        bmp->palData,bmp->palNum>0?bmp->palNum*2:0,pixels,linearSize)) goto done;
    count=state->variant==1?1:2;
    entries[0]=count==1?3:2;entries[1]=3;
    ys[0]=state->variant==1?6:4;ys[1]=state->variant==0?30:32;
    for(n=0;n<count;n++) {
        glyph=HuWinControlGlyphForPort(entries[n],state->port);
        if (!glyph) continue;
        if(glyph->bankNum==1 && glyph->patNum==1 &&
            (glyph->bmpNum & ANIM_BMP_NUM_MASK)==1 && glyph->bmp &&
            (glyph->bmp->sizeX==32 || glyph->bmp->sizeX==64) && glyph->bmp->sizeY==glyph->bmp->sizeX &&
            GlyphPanelDecode(glyph->bmp->dataFmt,glyph->bmp->sizeX,glyph->bmp->sizeY,glyph->bmp->data,glyph->bmp->dataSize,
                glyph->bmp->palData,glyph->bmp->palNum>0?glyph->bmp->palNum*2:0,glyphPixels,sizeof(glyphPixels))) {
            changed |= GlyphPanelStamp(pixels,bmp->sizeX,bmp->sizeY,linearSize,
                12,ys[n],24,42,glyphPixels,glyph->bmp->sizeX,glyph->bmp->sizeY,sizeof(glyphPixels));
        }
        HuSprAnimKill(glyph);
    }
    if (!changed) { if(HuSprData[sprite].data!=original)HuSprAnimationReplace(sprite,original,0);goto done; }
    made=HuSprAnimMake(bmp->sizeX,bmp->sizeY,ANIM_BMP_RGBA8);
    if (!made) goto done;
    /* Make returns an unretained animation. Own it during preparation. */
    HuSprAnimLock(made);
    made->bmp->dataSize=tiledSize;
    made->bmp->data=HuMemDirectMalloc(HEAP_DATA,tiledSize);
    if(!made->bmp->data)goto done;
    if(!GlyphPanelEncodeRGBA8(pixels,bmp->sizeX,bmp->sizeY,linearSize,made->bmp->data,tiledSize))goto done;
    HuSprAnimationReplace(sprite,made,0);
 done:
    if(made)HuSprAnimKill(made);
    HuMemDirectFree(pixels);
}
BOOL HuSprControlPanelSet(s16 sprite,s16 variant,s16 port) {
    ANIMDATA *anim;
    ANIMPAT *pat;
    static const s16 heights[3]={58,36,62};
    if(sprite<=0 || sprite>=HUSPR_MAX || variant<0 || variant>2 || port<0 || port>=4 ||
        controlGlyphSprites[sprite].original || (HuSprData[sprite].attr&HUSPR_ATTR_FUNC))return FALSE;
    if(controlPanelSprites[sprite].original)return FALSE;
    anim=HuSprData[sprite].data;
    if(!anim || anim->bankNum!=1 || anim->patNum!=1 || (anim->bmpNum&ANIM_BMP_NUM_MASK)!=1 ||
        !anim->bmp || !anim->bank || !anim->pat || anim->bank[0].timeNum!=1 || !anim->bank[0].frame ||
        anim->bank[0].frame[0].pat!=0 || anim->bmp[0].sizeX!=216 || anim->bmp[0].sizeY!=heights[variant])return FALSE;
    pat=anim->pat;
    if(pat->layerNum!=1 || !pat->layer || pat->sizeX!=216 || pat->sizeY!=heights[variant] ||
        pat->centerX!=108 || pat->centerY!=heights[variant]/2 || pat->layer->bmpNo!=0 ||
        pat->layer->startX || pat->layer->startY || pat->layer->flip || pat->layer->alpha!=255 ||
        pat->layer->sizeX!=216 || pat->layer->sizeY!=heights[variant])return FALSE;
    HuSprAnimLock(anim);
    controlPanelSprites[sprite].original=anim;
    controlPanelSprites[sprite].port=port;
    controlPanelSprites[sprite].variant=variant;
    controlPanelSprites[sprite].refresh=119;
    ControlPanelRefresh(sprite);
    return TRUE;
}
