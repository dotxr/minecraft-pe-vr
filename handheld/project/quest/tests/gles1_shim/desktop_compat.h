// Maps the GLES 1.1 names used by scene.cpp onto desktop GL 2.1 (compatibility profile),
// which has the same fixed-function model. Used only for the reference render.
#ifndef DESKTOP_COMPAT_H
#define DESKTOP_COMPAT_H
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#define glFrustumf(l, r, b, t, n, f) glFrustum((l), (r), (b), (t), (n), (f))
#define glOrthof(l, r, b, t, n, f) glOrtho((l), (r), (b), (t), (n), (f))
#define glFogx(p, v) glFogi((p), (v))
#define glClearDepthf(d) glClearDepth((d))
#define glDepthRangef(n, f) glDepthRange((n), (f))
#endif
