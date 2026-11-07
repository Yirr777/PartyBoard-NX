#ifndef PORT_WIDESCREEN_H
#define PORT_WIDESCREEN_H

// Anamorphic 16:9, the C version of Dolphin's "$16:9 Widescreen" Gecko code for GMPE01
// (Data/Sys/GameSettings/GMPE01.ini). The game still draws its 640x480 frame; Aurora stretches it
// to the screen (AURORA_VIEWPORT_STRETCH), so the game draws everything 3/4 as wide:
//  - 3D: every perspective is widened by 4/3 (__MTXAspectScale in Aurora's matrix library).
//  - 2D: the 576-wide sprite and window space becomes -96..672, so 0..576 is the middle 4:3 part;
//    the 640-wide print space becomes 0..853.
//  - HUD that the code moves to the edges goes through HuWideX.
// docs/port-notes.md lists each line of the code and where it is done here.

#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

extern SHARED_SYM int PartyBoard_Widescreen;

void PartyBoard_SetWidescreen(int enable);

#ifdef __cplusplus
}
#endif

#define HU_WIDE_SCALE (4.0f / 3.0f)
// The 2D (576-wide) screen's left and right edges.
#define HU_WIDE_DISP_LEFT (PartyBoard_Widescreen ? -96.0f : 0.0f)
#define HU_WIDE_DISP_RIGHT (PartyBoard_Widescreen ? 672.0f : 576.0f)
// The print (640-wide) screen's right edge.
#define HU_WIDE_FB_RIGHT (PartyBoard_Widescreen ? (640.0f * HU_WIDE_SCALE) : 640.0f)
// The 3D aspect scale.
#define HU_WIDE_ASPECT (PartyBoard_Widescreen ? HU_WIDE_SCALE : 1.0f)

// Moves a 2D x toward the screen edges, keeping the centre (288) in place.
static inline float HuWideX(float x)
{
    return PartyBoard_Widescreen ? x * HU_WIDE_SCALE - 96.0f : x;
}

// A 2D model-view matrix whose drawing spans the whole 576-wide screen (a menu backdrop, a movie)
// is stretched over the 16:9 width, so it has no side bars. left/right are the drawing's own x edges.
// Anything narrower, or rotated, keeps its shape.
static inline void HuWideBackdropMtx(float m[3][4], float left, float right)
{
    float x0, x1, t;
    if (!PartyBoard_Widescreen || m[0][1] != 0.0f) {
        return;
    }
    x0 = m[0][0] * left + m[0][3];
    x1 = m[0][0] * right + m[0][3];
    if (x0 > x1) {
        t = x0;
        x0 = x1;
        x1 = t;
    }
    if (x0 <= 8.0f && x1 >= 576.0f - 8.0f) {
        m[0][0] *= HU_WIDE_SCALE;
        m[0][2] *= HU_WIDE_SCALE;
        m[0][3] = (m[0][3] - 288.0f) * HU_WIDE_SCALE + 288.0f;
    }
}

#endif
