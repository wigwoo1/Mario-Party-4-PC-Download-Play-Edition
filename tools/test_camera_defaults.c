#include "game/hu3d.h"
#include "game/disp.h"
#include "game/init.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

extern HU3DCAMERA defCamera;
static GXRenderModeObj testMode;
GXRenderModeObj *RenderMode = &testMode;

int main(void) {
    Mtx44 projection;
    assert(defCamera.aspect == HU_DISP_ASPECT);
    assert(defCamera.upRot == 0.0f);
    testMode.fbWidth = 640;
    testMode.efbHeight = 480;
    Hu3DCameraCreate(1);
    assert(Hu3DCamera[0].aspect == HU_DISP_ASPECT);
    assert(Hu3DCamera[0].upRot == 0.0f);
    assert(Hu3DCamera[0].viewportW == 640);
    assert(Hu3DCamera[0].viewportH == 480);
    C_MTXPerspective(projection, defCamera.fov, defCamera.aspect,
                     defCamera.nnear, defCamera.ffar);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            assert(isfinite(projection[row][col]));
    puts("Actual default camera: aspect, rotation and finite perspective PASS");
    return 0;
}
