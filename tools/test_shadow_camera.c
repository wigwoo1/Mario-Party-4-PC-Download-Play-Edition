#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <dolphin/mtx.h>

static void check_view(Vec position, Vec target) {
    const Vec up = {0.0f, 1.0f, 0.0f};
    Vec old_up, right;
    Mtx view;
    C_VECNormalize(&position, &old_up);
    C_VECCrossProduct(&old_up, &old_up, &right);
    assert(C_VECSquareMag(&right) == 0.0f);
    C_MTXLookAt(view, &position, &up, &target);
    for (int row = 0; row < 3; ++row) {
        float length = 0.0f;
        float camera_space = view[row][3];
        for (int column = 0; column < 3; ++column) {
            assert(isfinite(view[row][column]));
            length += view[row][column] * view[row][column];
        }
        camera_space += view[row][0] * position.x;
        camera_space += view[row][1] * position.y;
        camera_space += view[row][2] * position.z;
        assert(fabsf(length - 1.0f) < 0.00001f);
        assert(fabsf(camera_space) < 0.001f);
        for (int other = row + 1; other < 3; ++other) {
            float dot = 0.0f;
            for (int column = 0; column < 3; ++column)
                dot += view[row][column] * view[other][column];
            assert(fabsf(dot) < 0.00001f);
        }
    }
    assert(view[0][0] > 0.99f);
    assert(view[1][1] > 0.0f);
}

static void check_camera(Vec position) {
    check_view(position, (Vec){0.0f, 0.0f, 0.0f});
}

int main(void) {
    check_camera((Vec){0.0f, 4000.0f, 1000.0f});
    check_camera((Vec){0.0f, 3000.0f, 1000.0f});
    check_camera((Vec){0.0f, 8000.0f, 1000.0f});
    check_camera((Vec){0.0f, 5000.0f, 100.0f});
    // M432's moving camera retains a 1000-unit z offset from its target.
    check_view((Vec){250.0f, 2000.0f, 700.0f}, (Vec){250.0f, 0.0f, -300.0f});
    check_view((Vec){-250.0f, 0.0f, 1400.0f}, (Vec){-250.0f, 0.0f, 400.0f});
    puts("PASS: actual scalar LookAt produces finite orthonormal M402, M441, M405, M429, M450 and M432 shadow cameras.");
    return 0;
}
