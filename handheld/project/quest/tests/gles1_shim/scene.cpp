// One GLES 1.1 scene, built twice: against Mesa's GLES 1.1 (reference) and against
// the shim on GLES 3. Only GLES 1.1 calls are made here. GL_QUADS is not part of
// GLES 1.1, so the reference build draws the same quads as triangles.
#ifdef REF_DESKTOP
#include "desktop_compat.h"
#else
#include <GLES/gl.h>
#endif
#include <cstring>
#include <cstdlib>

#ifndef GL_QUADS
#define GL_QUADS 0x0007  // not in GLES 1.1; the shim accepts it
#endif

#ifdef SHIM_BUILD
#define DRAW_QUADS(count) glDrawArrays(GL_QUADS, 0, (count))
#else
// two triangles per quad: indices 0,1,2 and 0,2,3, expanded to plain vertices by the caller
#define DRAW_QUADS(count) glDrawArrays(GL_TRIANGLES, 0, (count) / 4 * 6)
#endif

static void buildQuadStream(float* pos, float* uv, unsigned char* col, int quads, const float* quadPos, const float* quadUv) {
#ifdef SHIM_BUILD
    memcpy(pos, quadPos, sizeof(float) * 3 * 4 * quads);
    memcpy(uv, quadUv, sizeof(float) * 2 * 4 * quads);
#else
    for (int q = 0; q < quads; ++q) {
        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6; ++k) {
            int v = q * 4 + order[k];
            memcpy(pos + (q * 6 + k) * 3, quadPos + v * 3, sizeof(float) * 3);
            memcpy(uv + (q * 6 + k) * 2, quadUv + v * 2, sizeof(float) * 2);
        }
    }
#endif
    (void)col;
}

void renderScene(int w, int h) {
    const char* onlyEnv = getenv("SCENE_ONLY");
    const int only = onlyEnv ? atoi(onlyEnv) : 0;
    glViewport(0, 0, w, h);
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthRangef(0, 1);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-0.4f, 0.4f, -0.3f, 0.3f, 0.5f, 50.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -4);
    glRotatef(20, 1, 0, 0);
    glRotatef(35, 0, 1, 0);

    if (!only || (only >> 0) & 1) {  // 1
    // 1. Coloured triangle from client arrays, with a push/translate/pop around it.
    {
        static const float tri[] = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
        static const unsigned char triCol[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, tri);
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, triCol);
        glPushMatrix();
        glTranslatef(-1.2f, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glPopMatrix();
        glDisableClientState(GL_COLOR_ARRAY);
    }

    }
    if (!only || (only >> 1) & 1) {  // 2
    // 2. Textured quads with alpha test and the default MODULATE texture environment.
    {
        unsigned char tex[4 * 4 * 4];
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                unsigned char* p = tex + (y * 4 + x) * 4;
                bool dark = ((x ^ y) & 1) != 0;
                p[0] = dark ? 60 : 230;
                p[1] = dark ? 140 : 90;
                p[2] = dark ? 200 : 40;
                p[3] = (x == 0 && y == 0) ? 0 : 255;  // one transparent texel, killed by the alpha test
            }
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.5f);
        glColor4f(1.0f, 0.5f, 0.25f, 1.0f);

        const float quadPos[] = {
            -0.6f, 0.9f, -1.0f, -0.6f, 0.3f, -1.0f, 0.1f, 0.3f, -1.0f, 0.1f, 0.9f, -1.0f,
            -0.6f, -0.9f, -2.0f, -0.6f, -0.3f, -2.0f, 0.1f, -0.3f, -2.0f, 0.1f, -0.9f, -2.0f,
        };
        const float quadUv[] = {0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0};
        float pos[2 * 6 * 3];
        float uv[2 * 6 * 2];
        unsigned char dummy[1];
        buildQuadStream(pos, uv, dummy, 2, quadPos, quadUv);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, pos);
        glTexCoordPointer(2, GL_FLOAT, 0, uv);
#ifdef SHIM_BUILD
        // The renderer passes quads straight through, so use the 4-vertex form.
        glVertexPointer(3, GL_FLOAT, 0, quadPos);
        glTexCoordPointer(2, GL_FLOAT, 0, quadUv);
        DRAW_QUADS(8);
#else
        glDrawArrays(GL_TRIANGLES, 0, 12);
#endif
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
    }

    }
    if (!only || (only >> 2) & 1) {  // 3
    // 3. Linear fog over depth, drawn as a triangle strip receding into the distance.
    {
        glEnable(GL_FOG);
        glFogx(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 3.0f);
        glFogf(GL_FOG_END, 12.0f);
        const float fogc[4] = {0.8f, 0.8f, 0.9f, 1.0f};
        glFogfv(GL_FOG_COLOR, fogc);
        static const float strip[] = {
            -0.5f, -1.4f, -3.0f,  0.5f, -1.4f, -3.0f,
            -0.5f, -1.4f, -9.0f,  0.5f, -1.4f, -9.0f,
        };
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, strip);
        glColor4f(0.2f, 0.7f, 0.3f, 1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_FOG);
    }

    }
    if (!only || (only >> 3) & 1) {  // 4
    // 4. Interleaved VBO in the renderer's layout (pos, uv, colour), offsets in bytes.
    glDisable(GL_CULL_FACE);
    {
        struct V { float x, y, z, u, v; unsigned char c[4]; };
        V verts[6] = {
            {1.2f, 0.9f, -1.0f, 0, 0, {255, 255, 255, 255}},
            {1.9f, 0.9f, -1.0f, 1, 0, {255, 255, 255, 255}},
            {1.5f, 0.2f, -1.0f, 0.5f, 1, {255, 255, 255, 255}},
            {1.2f, -0.3f, -1.5f, 0, 0, {255, 128, 0, 255}},
            {1.9f, -0.3f, -1.5f, 1, 0, {255, 128, 0, 255}},
            {1.5f, -0.9f, -1.5f, 0.5f, 1, {255, 128, 0, 255}},
        };
        GLuint vbo = 0;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(V), (void*)0);
        glTexCoordPointer(2, GL_FLOAT, sizeof(V), (void*)12);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(V), (void*)20);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDrawArrays(GL_TRIANGLES, 3, 3);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &vbo);
    }
    glEnable(GL_CULL_FACE);

    }
    if (!only || (only >> 4) & 1) {  // 5
    // 5. Blended, depth-masked translucent quad in front of everything.
    glDisable(GL_CULL_FACE);
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glColor4f(1.0f, 1.0f, 0.0f, 0.5f);
        static const float fan[] = {-0.3f, 0.3f, -1.5f, 0.3f, 0.3f, -1.5f, 0.3f, -0.3f, -1.5f, -0.3f, -0.3f, -1.5f};
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, fan);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_CULL_FACE);

    }
    if (!only || (only >> 5) & 1) {  // 6
    // 6. Lines and a matrix-stack round trip that must leave the matrix unchanged.
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.5f, 0, 0);
        glPopMatrix();
        glLineWidth(2.0f);
        glColor4f(1, 1, 1, 1);
        static const float line[] = {-1.8f, -1.0f, -2.0f, 1.8f, -1.0f, -2.0f};
        glVertexPointer(3, GL_FLOAT, 0, line);
        glDrawArrays(GL_LINES, 0, 2);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    }
    glFinish();
}
