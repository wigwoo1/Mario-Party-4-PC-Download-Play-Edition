/* Included by hsfman.c only: owns glyph animation references until model teardown. */
typedef struct ModelControlGlyph_s {
    HSFBITMAP *bitmap;
    HSFDATA *hsf;
    HSFBITMAP original;
    ANIMDATA *glyph;
    s16 entry, port, refresh;
} ModelControlGlyph;
static ModelControlGlyph modelControlGlyphs[HU3D_MODEL_MAX];

static void ModelControlGlyphRestore(ModelControlGlyph *state)
{
#ifdef OPTIMIZED_TEXTURE_LOADING
    s16 i;
    if (state->hsf && state->bitmap) {
        for (i = 0; i < state->hsf->attributeNum; i++) {
            HSFATTRIBUTE *attribute = &state->hsf->attribute[i];
            if (attribute->bitmap != state->bitmap) continue;
            if (attribute->tex_initialized) GXDestroyTexObj(&attribute->tex_obj);
            if (attribute->tex8000_initialized) GXDestroyTexObj(&attribute->tex8000_obj);
            if (attribute->tlut_initialized) GXDestroyTlutObj(&attribute->tlut_obj);
            if (attribute->tlut8000_initialized) GXDestroyTlutObj(&attribute->tlut8000_obj);
            attribute->tex_initialized = attribute->tex8000_initialized = FALSE;
            attribute->tlut_initialized = attribute->tlut8000_initialized = FALSE;
        }
    }
#endif
    if (state->bitmap) *state->bitmap = state->original;
    if (state->glyph) HuSprAnimKill(state->glyph);
    state->glyph = NULL;
}

static void ModelControlGlyphKill(s16 model)
{
    ModelControlGlyph *state = &modelControlGlyphs[model];
    ModelControlGlyphRestore(state);
    memset(state, 0, sizeof(*state));
}

BOOL Hu3DModelControlGlyphSet(s16 model, const char *bitmapName, s16 entry, s16 port)
{
    ModelControlGlyph *state;
    HSFDATA *hsf;
    ANIMDATA *glyph;
    ANIMBMP *bitmap;
    s16 i;
    if (model < 0 || model >= HU3D_MODEL_MAX || !bitmapName) return FALSE;
    hsf = Hu3DData[model].hsf;
    if (!hsf || Hu3DData[model].linkMdlId != -1) return FALSE;
    state = &modelControlGlyphs[model];
    if (!state->bitmap) {
        /* Never mutate a bitmap shared with another model's HSF. */
        for (i = 0; i < HU3D_MODEL_MAX; i++) {
            if (i != model && (Hu3DData[i].hsf == hsf || Hu3DData[i].hsfLink == hsf)) return FALSE;
        }
        for (i = 0; i < hsf->bitmapNum; i++) {
            if (hsf->bitmap[i].name && strcmp(hsf->bitmap[i].name, bitmapName) == 0) {
                state->bitmap = &hsf->bitmap[i];
                state->hsf = hsf;
                state->original = *state->bitmap;
                state->refresh = 120;
                break;
            }
        }
        if (!state->bitmap) return FALSE;
    } else if (strcmp(state->original.name, bitmapName) != 0) {
        return FALSE; /* One explicitly selected bitmap per model. */
    }
    if (state->entry == entry && state->port == port && ++state->refresh < 120) return state->glyph != NULL;
    state->entry = entry; state->port = port; state->refresh = 0;
    glyph = HuWinControlGlyphForPort(entry, port);
    if (!glyph || glyph->bankNum != 1 || glyph->patNum != 1 ||
        (glyph->bmpNum & 0x7FFF) != 1 || !glyph->bmp) {
        if (glyph) HuSprAnimKill(glyph);
        ModelControlGlyphRestore(state);
        return FALSE;
    }
    bitmap = &glyph->bmp[0];
    if (bitmap->dataFmt != ANIM_BMP_RGBA8 || bitmap->pixSize != 32 || !bitmap->data ||
        bitmap->sizeX <= 0 || bitmap->sizeX > 256 || bitmap->sizeY <= 0 || bitmap->sizeY > 256 ||
        bitmap->sizeX % 4 || bitmap->sizeY % 4 ||
        bitmap->dataSize < (u32)bitmap->sizeX * bitmap->sizeY * 4) {
        HuSprAnimKill(glyph);
        ModelControlGlyphRestore(state);
        return FALSE;
    }
    ModelControlGlyphRestore(state);
    state->glyph = glyph;
    state->bitmap->dataFmt = HSF_BMPFMT_RGBA8;
    state->bitmap->pixSize = 32;
    state->bitmap->sizeX = bitmap->sizeX; state->bitmap->sizeY = bitmap->sizeY;
    state->bitmap->maxLod = 0; state->bitmap->palSize = 0;
    state->bitmap->palData = NULL; state->bitmap->data = bitmap->data;
    return TRUE;
}
