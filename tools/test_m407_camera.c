#include "../src/REL/m407dll/camera.c"
#include "../src/game/objsysobj.c"
#include <assert.h>
#include <stdio.h>

static int submitted;
void Hu3DCameraPosSet(s32 camera, f32 x, f32 y, f32 z,
                     f32 ux, f32 uy, f32 uz, f32 tx, f32 ty, f32 tz) {
    assert(camera == 1);
    const float dx = x - tx, dy = y - ty, dz = z - tz;
    assert(dx*dx + dy*dy + dz*dz > 0);
    assert(ux*ux + uy*uy + uz*uz > 0);
    submitted++;
}

int main(void) {
    unkDominationData2 state = {0};
    omObjData object = {0};
    object.data = &state;
    lbl_1_bss_30 = &object;
    fn_1_2024(&object);
    assert(submitted == 0);
    Vec target = {540, 0, 10000}, rotation = {-50, -360, 0};
    float zoom = 2670;
    fn_1_1DB0(&target, &rotation, &zoom, 0, 0);
    fn_1_2024(&object);
    assert(submitted == 1 && CZoom == zoom && state.unk_04 == 0);
    assert(Center.x == target.x && Center.z == target.z);
    assert(CRot.x == rotation.x && CRot.y == rotation.y);
    fn_1_2024(&object);
    assert(submitted == 2 && CZoom == zoom);
    target.x = 437;
    zoom = 2290;
    fn_1_1DB0(&target, NULL, &zoom, 2, 0);
    fn_1_2024(&object);
    assert(submitted == 3 && CZoom == 2670 && state.unk_04 == 1);
    fn_1_2024(&object);
    assert(submitted == 4 && CZoom == 2290 && state.unk_04 == 0);
    assert(Center.x == 437 && CRot.x == rotation.x);
    puts("PASS actual M407 camera: retain created pose on first update, apply zero-duration initial transition, submit subsequent updates.");
    return 0;
}
