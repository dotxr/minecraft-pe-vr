// GLES 1.1 fixed-function API implemented on top of OpenGL ES 3.0.
//
// The renderer was written against GLES 1.1 (glMatrixMode, glVertexPointer,
// glFog*, glAlphaFunc, ...). OpenXR only gives us a GLES 3 context, so this file
// provides those entry points and translates them into a handful of GLES 3
// calls:
//   * matrices live on the CPU and are uploaded as uniforms,
//   * fixed-function state (texturing, alpha test, fog) drives one shader,
//   * client-side vertex arrays are copied into streaming buffers,
//   * GL_QUADS is drawn through a shared index buffer (GLES 3 has no quads).
//
// Lighting is not implemented: the renderer never enables it.
//
// The real GLES 3 entry points are fetched with eglGetProcAddress instead of
// being linked. Many GLES 1 names (glEnable, glDrawArrays, glBindBuffer, ...)
// also exist in GLES 3, so linking libGLESv3 would collide with the definitions
// below.

#include "gles1_shim.h"

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/log.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#define SHIM_LOG(...) __android_log_print(ANDROID_LOG_INFO, "gles1-shim", __VA_ARGS__)
#define SHIM_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "gles1-shim", __VA_ARGS__)

// Fixed-function enums that GLES 1 has and GLES 3 does not.
#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#ifndef GL_FOG
#define GL_FOG 0x0B60
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif
#ifndef GL_LIGHTING
#define GL_LIGHTING 0x0B50
#endif

namespace {

// ---------------------------------------------------------------------------
// Real GLES 3 entry points, resolved at runtime.
// ---------------------------------------------------------------------------
struct Gl3 {
    void (GL_APIENTRY *Enable)(GLenum);
    void (GL_APIENTRY *Disable)(GLenum);
    void (GL_APIENTRY *BindTexture)(GLenum, GLuint);
    void (GL_APIENTRY *TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void (GL_APIENTRY *TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
    void (GL_APIENTRY *TexParameteri)(GLenum, GLenum, GLint);
    void (GL_APIENTRY *GenTextures)(GLsizei, GLuint*);
    void (GL_APIENTRY *DeleteTextures)(GLsizei, const GLuint*);
    void (GL_APIENTRY *ActiveTexture)(GLenum);
    void (GL_APIENTRY *BindBuffer)(GLenum, GLuint);
    void (GL_APIENTRY *BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void (GL_APIENTRY *GenBuffers)(GLsizei, GLuint*);
    void (GL_APIENTRY *DeleteBuffers)(GLsizei, const GLuint*);
    void (GL_APIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    void (GL_APIENTRY *DrawElements)(GLenum, GLsizei, GLenum, const void*);
    void (GL_APIENTRY *ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GL_APIENTRY *ClearDepthf)(GLfloat);
    void (GL_APIENTRY *Clear)(GLbitfield);
    void (GL_APIENTRY *ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (GL_APIENTRY *DepthMask)(GLboolean);
    void (GL_APIENTRY *DepthFunc)(GLenum);
    void (GL_APIENTRY *DepthRangef)(GLfloat, GLfloat);
    void (GL_APIENTRY *BlendFunc)(GLenum, GLenum);
    void (GL_APIENTRY *CullFace)(GLenum);
    void (GL_APIENTRY *FrontFace)(GLenum);
    void (GL_APIENTRY *Viewport)(GLint, GLint, GLsizei, GLsizei);
    void (GL_APIENTRY *Scissor)(GLint, GLint, GLsizei, GLsizei);
    void (GL_APIENTRY *PolygonOffset)(GLfloat, GLfloat);
    void (GL_APIENTRY *LineWidth)(GLfloat);
    void (GL_APIENTRY *PixelStorei)(GLenum, GLint);
    GLenum (GL_APIENTRY *GetError)(void);
    const GLubyte* (GL_APIENTRY *GetString)(GLenum);
    void (GL_APIENTRY *GetIntegerv)(GLenum, GLint*);
    void (GL_APIENTRY *GetFloatv)(GLenum, GLfloat*);
    void (GL_APIENTRY *ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void (GL_APIENTRY *VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    void (GL_APIENTRY *EnableVertexAttribArray)(GLuint);
    void (GL_APIENTRY *DisableVertexAttribArray)(GLuint);
    void (GL_APIENTRY *VertexAttrib4f)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (GL_APIENTRY *BindAttribLocation)(GLuint, GLuint, const GLchar*);
    GLuint (GL_APIENTRY *CreateShader)(GLenum);
    void (GL_APIENTRY *ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    void (GL_APIENTRY *CompileShader)(GLuint);
    void (GL_APIENTRY *GetShaderiv)(GLuint, GLenum, GLint*);
    void (GL_APIENTRY *GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    GLuint (GL_APIENTRY *CreateProgram)(void);
    void (GL_APIENTRY *AttachShader)(GLuint, GLuint);
    void (GL_APIENTRY *LinkProgram)(GLuint);
    void (GL_APIENTRY *GetProgramiv)(GLuint, GLenum, GLint*);
    void (GL_APIENTRY *GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (GL_APIENTRY *UseProgram)(GLuint);
    GLint (GL_APIENTRY *GetUniformLocation)(GLuint, const GLchar*);
    void (GL_APIENTRY *Uniform1i)(GLint, GLint);
    void (GL_APIENTRY *Uniform1f)(GLint, GLfloat);
    void (GL_APIENTRY *Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (GL_APIENTRY *UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    void (GL_APIENTRY *DeleteShader)(GLuint);
    void (GL_APIENTRY *DeleteProgram)(GLuint);
};

Gl3 gl3;

template <typename T>
void loadGl3(T& fn, const char* name) {
    fn = reinterpret_cast<T>(eglGetProcAddress(name));
    if (!fn) SHIM_LOGE("missing GLES 3 entry point %s", name);
}

void loadGl3Table() {
    loadGl3(gl3.Enable, "glEnable");
    loadGl3(gl3.Disable, "glDisable");
    loadGl3(gl3.BindTexture, "glBindTexture");
    loadGl3(gl3.TexImage2D, "glTexImage2D");
    loadGl3(gl3.TexSubImage2D, "glTexSubImage2D");
    loadGl3(gl3.TexParameteri, "glTexParameteri");
    loadGl3(gl3.GenTextures, "glGenTextures");
    loadGl3(gl3.DeleteTextures, "glDeleteTextures");
    loadGl3(gl3.ActiveTexture, "glActiveTexture");
    loadGl3(gl3.BindBuffer, "glBindBuffer");
    loadGl3(gl3.BufferData, "glBufferData");
    loadGl3(gl3.GenBuffers, "glGenBuffers");
    loadGl3(gl3.DeleteBuffers, "glDeleteBuffers");
    loadGl3(gl3.DrawArrays, "glDrawArrays");
    loadGl3(gl3.DrawElements, "glDrawElements");
    loadGl3(gl3.ClearColor, "glClearColor");
    loadGl3(gl3.ClearDepthf, "glClearDepthf");
    loadGl3(gl3.Clear, "glClear");
    loadGl3(gl3.ColorMask, "glColorMask");
    loadGl3(gl3.DepthMask, "glDepthMask");
    loadGl3(gl3.DepthFunc, "glDepthFunc");
    loadGl3(gl3.DepthRangef, "glDepthRangef");
    loadGl3(gl3.BlendFunc, "glBlendFunc");
    loadGl3(gl3.CullFace, "glCullFace");
    loadGl3(gl3.FrontFace, "glFrontFace");
    loadGl3(gl3.Viewport, "glViewport");
    loadGl3(gl3.Scissor, "glScissor");
    loadGl3(gl3.PolygonOffset, "glPolygonOffset");
    loadGl3(gl3.LineWidth, "glLineWidth");
    loadGl3(gl3.PixelStorei, "glPixelStorei");
    loadGl3(gl3.GetError, "glGetError");
    loadGl3(gl3.GetString, "glGetString");
    loadGl3(gl3.GetIntegerv, "glGetIntegerv");
    loadGl3(gl3.GetFloatv, "glGetFloatv");
    loadGl3(gl3.ReadPixels, "glReadPixels");
    loadGl3(gl3.VertexAttribPointer, "glVertexAttribPointer");
    loadGl3(gl3.EnableVertexAttribArray, "glEnableVertexAttribArray");
    loadGl3(gl3.DisableVertexAttribArray, "glDisableVertexAttribArray");
    loadGl3(gl3.VertexAttrib4f, "glVertexAttrib4f");
    loadGl3(gl3.BindAttribLocation, "glBindAttribLocation");
    loadGl3(gl3.CreateShader, "glCreateShader");
    loadGl3(gl3.ShaderSource, "glShaderSource");
    loadGl3(gl3.CompileShader, "glCompileShader");
    loadGl3(gl3.GetShaderiv, "glGetShaderiv");
    loadGl3(gl3.GetShaderInfoLog, "glGetShaderInfoLog");
    loadGl3(gl3.CreateProgram, "glCreateProgram");
    loadGl3(gl3.AttachShader, "glAttachShader");
    loadGl3(gl3.LinkProgram, "glLinkProgram");
    loadGl3(gl3.GetProgramiv, "glGetProgramiv");
    loadGl3(gl3.GetProgramInfoLog, "glGetProgramInfoLog");
    loadGl3(gl3.UseProgram, "glUseProgram");
    loadGl3(gl3.GetUniformLocation, "glGetUniformLocation");
    loadGl3(gl3.Uniform1i, "glUniform1i");
    loadGl3(gl3.Uniform1f, "glUniform1f");
    loadGl3(gl3.Uniform4f, "glUniform4f");
    loadGl3(gl3.UniformMatrix4fv, "glUniformMatrix4fv");
    loadGl3(gl3.DeleteShader, "glDeleteShader");
    loadGl3(gl3.DeleteProgram, "glDeleteProgram");
}

// ---------------------------------------------------------------------------
// Fixed-function state.
// ---------------------------------------------------------------------------
const float kPi = 3.14159265358979323846f;
const int kMaxModelViewDepth = 32;
const int kMaxProjectionDepth = 4;

// Column-major 4x4, same layout as GL.
struct Mat {
    float m[16];
};

void matIdentity(float* m) {
    memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// out = a * b
void matMul(const float* a, const float* b, float* out) {
    float r[16];
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0]
                           + a[1 * 4 + row] * b[c * 4 + 1]
                           + a[2 * 4 + row] * b[c * 4 + 2]
                           + a[3 * 4 + row] * b[c * 4 + 3];
        }
    }
    memcpy(out, r, sizeof(r));
}

void matTranslate(float* m, float x, float y, float z) {
    float t[16];
    matIdentity(t);
    t[12] = x; t[13] = y; t[14] = z;
    matMul(m, t, m);
}

void matScale(float* m, float x, float y, float z) {
    float s[16];
    matIdentity(s);
    s[0] = x; s[5] = y; s[10] = z;
    matMul(m, s, m);
}

void matRotate(float* m, float angleDeg, float x, float y, float z) {
    float len = std::sqrt(x * x + y * y + z * z);
    if (len == 0.0f) return;
    x /= len; y /= len; z /= len;
    float rad = angleDeg * kPi / 180.0f;
    float c = std::cos(rad);
    float s = std::sin(rad);
    float t = 1.0f - c;
    float r[16];
    matIdentity(r);
    r[0] = t * x * x + c;     r[4] = t * x * y - s * z; r[8]  = t * x * z + s * y;
    r[1] = t * x * y + s * z; r[5] = t * y * y + c;     r[9]  = t * y * z - s * x;
    r[2] = t * x * z - s * y; r[6] = t * y * z + s * x; r[10] = t * z * z + c;
    matMul(m, r, m);
}

void matFrustum(float* m, float l, float r, float b, float t, float n, float f) {
    float p[16];
    memset(p, 0, sizeof(p));
    p[0]  = 2.0f * n / (r - l);
    p[5]  = 2.0f * n / (t - b);
    p[8]  = (r + l) / (r - l);
    p[9]  = (t + b) / (t - b);
    p[10] = -(f + n) / (f - n);
    p[11] = -1.0f;
    p[14] = -2.0f * f * n / (f - n);
    matMul(m, p, m);
}

void matOrtho(float* m, float l, float r, float b, float t, float n, float f) {
    float p[16];
    matIdentity(p);
    p[0]  = 2.0f / (r - l);
    p[5]  = 2.0f / (t - b);
    p[10] = -2.0f / (f - n);
    p[12] = -(r + l) / (r - l);
    p[13] = -(t + b) / (t - b);
    p[14] = -(f + n) / (f - n);
    matMul(m, p, m);
}

struct ClientArray {
    bool enabled;
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid* pointer;  // client address, or byte offset when buffer != 0
    GLuint buffer;          // GL_ARRAY_BUFFER bound when the pointer was set
};

// Attribute slots used by the generated shader.
enum { kAttribPosition = 0, kAttribColor = 1, kAttribTexCoord = 2, kAttribCount = 3 };

struct ShimState {
    bool initialised;
    GLenum matrixMode;
    Mat modelView[kMaxModelViewDepth];
    int modelViewDepth;
    Mat projection[kMaxProjectionDepth];
    int projectionDepth;
    Mat texture;

    float color[4];

    bool texture2D;
    bool alphaTest;
    GLenum alphaFunc;
    float alphaRef;

    bool fog;
    GLenum fogMode;
    float fogDensity;
    float fogStart;
    float fogEnd;
    float fogColor[4];

    ClientArray arrays[kAttribCount];
    GLuint arrayBuffer;     // what the game last bound to GL_ARRAY_BUFFER

    GLuint program;
    GLint locModelView, locProjection, locTexture, locSampler;
    GLint locTexOn, locAlphaOn, locAlphaFunc, locAlphaRef;
    GLint locFogOn, locFogMode, locFogStart, locFogEnd, locFogDensity, locFogColor;

    GLuint scratch[kAttribCount];   // streaming buffers for client-memory arrays
    GLuint quadIndices;
    GLsizei quadIndexCapacity;      // in quads
};

ShimState gs;

const char* kVertexShader =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 aPosition;\n"
    "in vec4 aColor;\n"
    "in vec2 aTexCoord;\n"
    "uniform mat4 uModelView;\n"
    "uniform mat4 uProjection;\n"
    "uniform mat4 uTexture;\n"
    "out vec4 vColor;\n"
    "out vec2 vTexCoord;\n"
    "out float vFogDistance;\n"
    "void main() {\n"
    "    vec4 eye = uModelView * aPosition;\n"
    "    vFogDistance = abs(eye.z);\n"
    "    gl_Position = uProjection * eye;\n"
    "    vColor = aColor;\n"
    "    vTexCoord = (uTexture * vec4(aTexCoord, 0.0, 1.0)).xy;\n"
    "}\n";

const char* kFragmentShader =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec4 vColor;\n"
    "in vec2 vTexCoord;\n"
    "in float vFogDistance;\n"
    "uniform sampler2D uSampler;\n"
    "uniform int uTexOn;\n"
    "uniform int uAlphaOn;\n"
    "uniform int uAlphaFunc;\n"
    "uniform float uAlphaRef;\n"
    "uniform int uFogOn;\n"
    "uniform int uFogMode;\n"
    "uniform float uFogStart;\n"
    "uniform float uFogEnd;\n"
    "uniform float uFogDensity;\n"
    "uniform vec4 uFogColor;\n"
    "out vec4 fragColor;\n"
    "bool alphaPasses(float a) {\n"
    "    if (uAlphaFunc == 0x200) return false;\n"          // GL_NEVER
    "    if (uAlphaFunc == 0x201) return a < uAlphaRef;\n"  // GL_LESS
    "    if (uAlphaFunc == 0x202) return a == uAlphaRef;\n" // GL_EQUAL
    "    if (uAlphaFunc == 0x203) return a <= uAlphaRef;\n" // GL_LEQUAL
    "    if (uAlphaFunc == 0x204) return a > uAlphaRef;\n"  // GL_GREATER
    "    if (uAlphaFunc == 0x205) return a != uAlphaRef;\n" // GL_NOTEQUAL
    "    if (uAlphaFunc == 0x206) return a >= uAlphaRef;\n" // GL_GEQUAL
    "    return true;\n"                                    // GL_ALWAYS
    "}\n"
    "void main() {\n"
    "    vec4 c = vColor;\n"
    "    if (uTexOn != 0) c *= texture(uSampler, vTexCoord);\n"
    "    if (uAlphaOn != 0 && !alphaPasses(c.a)) discard;\n"
    "    if (uFogOn != 0) {\n"
    "        float f;\n"
    "        if (uFogMode == 0x2601) f = (uFogEnd - vFogDistance) / (uFogEnd - uFogStart);\n"  // GL_LINEAR
    "        else if (uFogMode == 0x0800) f = exp(-uFogDensity * vFogDistance);\n"           // GL_EXP
    "        else f = exp(-pow(uFogDensity * vFogDistance, 2.0));\n"                         // GL_EXP2
    "        c.rgb = mix(uFogColor.rgb, c.rgb, clamp(f, 0.0, 1.0));\n"
    "    }\n"
    "    fragColor = c;\n"
    "}\n";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = gl3.CreateShader(type);
    gl3.ShaderSource(shader, 1, &source, NULL);
    gl3.CompileShader(shader);
    GLint ok = 0;
    gl3.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        gl3.GetShaderInfoLog(shader, sizeof(log), NULL, log);
        SHIM_LOGE("shader compile failed: %s", log);
    }
    return shader;
}

void initProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    GLuint prog = gl3.CreateProgram();
    gl3.AttachShader(prog, vs);
    gl3.AttachShader(prog, fs);
    gl3.BindAttribLocation(prog, kAttribPosition, "aPosition");
    gl3.BindAttribLocation(prog, kAttribColor, "aColor");
    gl3.BindAttribLocation(prog, kAttribTexCoord, "aTexCoord");
    gl3.LinkProgram(prog);
    GLint ok = 0;
    gl3.GetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        gl3.GetProgramInfoLog(prog, sizeof(log), NULL, log);
        SHIM_LOGE("program link failed: %s", log);
    }
    gl3.DeleteShader(vs);
    gl3.DeleteShader(fs);

    gs.program = prog;
    gs.locModelView = gl3.GetUniformLocation(prog, "uModelView");
    gs.locProjection = gl3.GetUniformLocation(prog, "uProjection");
    gs.locTexture = gl3.GetUniformLocation(prog, "uTexture");
    gs.locSampler = gl3.GetUniformLocation(prog, "uSampler");
    gs.locTexOn = gl3.GetUniformLocation(prog, "uTexOn");
    gs.locAlphaOn = gl3.GetUniformLocation(prog, "uAlphaOn");
    gs.locAlphaFunc = gl3.GetUniformLocation(prog, "uAlphaFunc");
    gs.locAlphaRef = gl3.GetUniformLocation(prog, "uAlphaRef");
    gs.locFogOn = gl3.GetUniformLocation(prog, "uFogOn");
    gs.locFogMode = gl3.GetUniformLocation(prog, "uFogMode");
    gs.locFogStart = gl3.GetUniformLocation(prog, "uFogStart");
    gs.locFogEnd = gl3.GetUniformLocation(prog, "uFogEnd");
    gs.locFogDensity = gl3.GetUniformLocation(prog, "uFogDensity");
    gs.locFogColor = gl3.GetUniformLocation(prog, "uFogColor");
}

void ensureInitialised() {
    if (gs.initialised) return;
    gs.initialised = true;

    loadGl3Table();

    gs.matrixMode = GL_MODELVIEW;
    matIdentity(gs.modelView[0].m);
    gs.modelViewDepth = 0;
    matIdentity(gs.projection[0].m);
    gs.projectionDepth = 0;
    matIdentity(gs.texture.m);

    gs.color[0] = gs.color[1] = gs.color[2] = gs.color[3] = 1.0f;
    gs.texture2D = false;
    gs.alphaTest = false;
    gs.alphaFunc = GL_ALWAYS;
    gs.alphaRef = 0.0f;
    gs.fog = false;
    gs.fogMode = GL_EXP;
    gs.fogDensity = 1.0f;
    gs.fogStart = 0.0f;
    gs.fogEnd = 1.0f;
    gs.fogColor[0] = gs.fogColor[1] = gs.fogColor[2] = gs.fogColor[3] = 0.0f;

    for (int i = 0; i < kAttribCount; ++i) {
        gs.arrays[i].enabled = false;
        gs.arrays[i].size = 4;
        gs.arrays[i].type = GL_FLOAT;
        gs.arrays[i].stride = 0;
        gs.arrays[i].pointer = NULL;
        gs.arrays[i].buffer = 0;
        gs.scratch[i] = 0;
    }
    gs.arrayBuffer = 0;
    gs.quadIndices = 0;
    gs.quadIndexCapacity = 0;

    initProgram();
}

size_t typeSize(GLenum type) {
    switch (type) {
        case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
        case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
        default: return 4;  // GL_FLOAT, GL_FIXED
    }
}

// Last values sent to the shader, so unchanged uniforms are not re-uploaded. The
// game issues thousands of draws per frame and most share the same state.
struct UploadedState {
    bool valid;
    float modelView[16];
    float projection[16];
    float texture[16];
    int texOn, alphaOn, alphaFunc, fogOn, fogMode;
    float alphaRef, fogStart, fogEnd, fogDensity;
    float fogColor[4];
    float color[4];
};
UploadedState gUploaded;

template <typename T>
bool changed(T& last, const T& now) {
    if (last == now) return false;
    last = now;
    return true;
}

// Uploads everything the shader needs for the next draw.
void flushState() {
    ensureInitialised();
    gl3.UseProgram(gs.program);

    UploadedState& u = gUploaded;
    const float* mv = gs.modelView[gs.modelViewDepth].m;
    const float* proj = gs.projection[gs.projectionDepth].m;
    if (!u.valid || memcmp(u.modelView, mv, sizeof(u.modelView)) != 0) {
        memcpy(u.modelView, mv, sizeof(u.modelView));
        gl3.UniformMatrix4fv(gs.locModelView, 1, GL_FALSE, mv);
    }
    if (!u.valid || memcmp(u.projection, proj, sizeof(u.projection)) != 0) {
        memcpy(u.projection, proj, sizeof(u.projection));
        gl3.UniformMatrix4fv(gs.locProjection, 1, GL_FALSE, proj);
    }
    if (!u.valid || memcmp(u.texture, gs.texture.m, sizeof(u.texture)) != 0) {
        memcpy(u.texture, gs.texture.m, sizeof(u.texture));
        gl3.UniformMatrix4fv(gs.locTexture, 1, GL_FALSE, gs.texture.m);
    }

    int texOn = gs.texture2D ? 1 : 0;
    int alphaOn = gs.alphaTest ? 1 : 0;
    int alphaFunc = (int)gs.alphaFunc;
    int fogOn = gs.fog ? 1 : 0;
    int fogMode = (int)gs.fogMode;
    if (!u.valid || changed(u.texOn, texOn)) gl3.Uniform1i(gs.locTexOn, texOn);
    if (!u.valid || changed(u.alphaOn, alphaOn)) gl3.Uniform1i(gs.locAlphaOn, alphaOn);
    if (!u.valid || changed(u.alphaFunc, alphaFunc)) gl3.Uniform1i(gs.locAlphaFunc, alphaFunc);
    if (!u.valid || changed(u.alphaRef, gs.alphaRef)) gl3.Uniform1f(gs.locAlphaRef, gs.alphaRef);
    if (!u.valid || changed(u.fogOn, fogOn)) gl3.Uniform1i(gs.locFogOn, fogOn);
    if (!u.valid || changed(u.fogMode, fogMode)) gl3.Uniform1i(gs.locFogMode, fogMode);
    if (!u.valid || changed(u.fogStart, gs.fogStart)) gl3.Uniform1f(gs.locFogStart, gs.fogStart);
    if (!u.valid || changed(u.fogEnd, gs.fogEnd)) gl3.Uniform1f(gs.locFogEnd, gs.fogEnd);
    if (!u.valid || changed(u.fogDensity, gs.fogDensity)) gl3.Uniform1f(gs.locFogDensity, gs.fogDensity);
    if (!u.valid || memcmp(u.fogColor, gs.fogColor, sizeof(u.fogColor)) != 0) {
        memcpy(u.fogColor, gs.fogColor, sizeof(u.fogColor));
        gl3.Uniform4f(gs.locFogColor, gs.fogColor[0], gs.fogColor[1], gs.fogColor[2], gs.fogColor[3]);
    }
    if (!u.valid) gl3.Uniform1i(gs.locSampler, 0);

    // Used when the colour array is disabled.
    if (!u.valid || memcmp(u.color, gs.color, sizeof(u.color)) != 0) {
        memcpy(u.color, gs.color, sizeof(u.color));
        gl3.VertexAttrib4f(kAttribColor, gs.color[0], gs.color[1], gs.color[2], gs.color[3]);
    }
    u.valid = true;
}

// Points the three shader attributes at either the game's buffers or freshly
// staged copies of client-memory arrays. Vertices are addressed from 0 after this.
void bindArrays(GLint first, GLsizei count) {
    for (int i = 0; i < kAttribCount; ++i) {
        ClientArray& a = gs.arrays[i];
        if (!a.enabled) {
            gl3.DisableVertexAttribArray(i);
            continue;
        }

        size_t elem = typeSize(a.type) * (size_t)a.size;
        if (i == kAttribColor) elem = typeSize(a.type) * 4;  // colour is always 4 wide
        GLsizei stride = a.stride ? a.stride : (GLsizei)elem;
        GLboolean normalised = (i == kAttribColor && a.type == GL_UNSIGNED_BYTE) ? GL_TRUE : GL_FALSE;
        GLint size = (i == kAttribColor) ? 4 : a.size;

        if (a.buffer) {
            size_t offset = (size_t)(uintptr_t)a.pointer + (size_t)first * (size_t)stride;
            gl3.BindBuffer(GL_ARRAY_BUFFER, a.buffer);
            gl3.VertexAttribPointer(i, size, a.type, normalised, stride, (const void*)offset);
        } else {
            if (!gs.scratch[i]) gl3.GenBuffers(1, &gs.scratch[i]);
            const char* src = (const char*)a.pointer + (size_t)first * (size_t)stride;
            gl3.BindBuffer(GL_ARRAY_BUFFER, gs.scratch[i]);
            gl3.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)count * (size_t)stride), src, GL_STREAM_DRAW);
            gl3.VertexAttribPointer(i, size, a.type, normalised, stride, NULL);
        }
        gl3.EnableVertexAttribArray(i);
    }
    gl3.BindBuffer(GL_ARRAY_BUFFER, gs.arrayBuffer);
}

// Index buffer for drawing quads as two triangles each.
void ensureQuadIndices(GLsizei quads) {
    if (quads <= gs.quadIndexCapacity) return;

    GLsizei capacity = quads * 2;
    if (capacity < 1024) capacity = 1024;
    std::vector<GLuint> indices((size_t)capacity * 6);
    for (GLsizei q = 0; q < capacity; ++q) {
        GLuint b = (GLuint)q * 4;
        GLuint* out = &indices[(size_t)q * 6];
        out[0] = b;     out[1] = b + 1; out[2] = b + 2;
        out[3] = b;     out[4] = b + 2; out[5] = b + 3;
    }

    if (!gs.quadIndices) gl3.GenBuffers(1, &gs.quadIndices);
    gl3.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gs.quadIndices);
    gl3.BufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(GLuint)), &indices[0], GL_STATIC_DRAW);
    gl3.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gs.quadIndexCapacity = capacity;
}

Mat* currentMatrix() {
    if (gs.matrixMode == GL_PROJECTION) return &gs.projection[gs.projectionDepth];
    if (gs.matrixMode == GL_TEXTURE) return &gs.texture;
    return &gs.modelView[gs.modelViewDepth];
}

void setFogParam(GLenum pname, const float* params) {
    switch (pname) {
        case GL_FOG_MODE: gs.fogMode = (GLenum)(GLint)params[0]; break;
        case GL_FOG_DENSITY: gs.fogDensity = params[0]; break;
        case GL_FOG_START: gs.fogStart = params[0]; break;
        case GL_FOG_END: gs.fogEnd = params[0]; break;
        case GL_FOG_COLOR:
            gs.fogColor[0] = params[0]; gs.fogColor[1] = params[1];
            gs.fogColor[2] = params[2]; gs.fogColor[3] = params[3];
            break;
        default: break;
    }
}

// Enum-only state that GLES 3 rejects; accepted and ignored.
bool isIgnoredCap(GLenum cap) {
    switch (cap) {
        case GL_LIGHTING: case GL_COLOR_MATERIAL: case GL_NORMALIZE: case GL_RESCALE_NORMAL:
        case GL_LINE_SMOOTH: case GL_POINT_SMOOTH: case GL_POINT_SPRITE_OES:
            return true;
        default:
            return cap >= GL_LIGHT0 && cap <= GL_LIGHT0 + 7;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
void gles1ShimInit() {
    ensureInitialised();
}

// ---------------------------------------------------------------------------
// GLES 1.1 entry points
// ---------------------------------------------------------------------------
extern "C" {

// Matrices
void glMatrixMode(GLenum mode) { ensureInitialised(); gs.matrixMode = mode; }

void glLoadIdentity(void) { matIdentity(currentMatrix()->m); }

void glLoadMatrixf(const GLfloat* m) { memcpy(currentMatrix()->m, m, sizeof(float) * 16); }

void glMultMatrixf(const GLfloat* m) { Mat* cur = currentMatrix(); matMul(cur->m, m, cur->m); }

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { matTranslate(currentMatrix()->m, x, y, z); }

void glScalef(GLfloat x, GLfloat y, GLfloat z) { matScale(currentMatrix()->m, x, y, z); }

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { matRotate(currentMatrix()->m, angle, x, y, z); }

void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    matFrustum(currentMatrix()->m, l, r, b, t, n, f);
}

void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    matOrtho(currentMatrix()->m, l, r, b, t, n, f);
}

void glPushMatrix(void) {
    ensureInitialised();
    if (gs.matrixMode == GL_PROJECTION) {
        if (gs.projectionDepth + 1 < kMaxProjectionDepth) {
            gs.projection[gs.projectionDepth + 1] = gs.projection[gs.projectionDepth];
            ++gs.projectionDepth;
        }
    } else if (gs.matrixMode == GL_MODELVIEW) {
        if (gs.modelViewDepth + 1 < kMaxModelViewDepth) {
            gs.modelView[gs.modelViewDepth + 1] = gs.modelView[gs.modelViewDepth];
            ++gs.modelViewDepth;
        } else {
            SHIM_LOGE("modelview stack overflow");
        }
    }
}

void glPopMatrix(void) {
    ensureInitialised();
    if (gs.matrixMode == GL_PROJECTION) {
        if (gs.projectionDepth > 0) --gs.projectionDepth;
    } else if (gs.matrixMode == GL_MODELVIEW) {
        if (gs.modelViewDepth > 0) --gs.modelViewDepth;
        else SHIM_LOGE("modelview stack underflow");
    }
}

void glGetFloatv(GLenum pname, GLfloat* params) {
    ensureInitialised();
    if (pname == GL_MODELVIEW_MATRIX) {
        memcpy(params, gs.modelView[gs.modelViewDepth].m, sizeof(float) * 16);
    } else if (pname == GL_PROJECTION_MATRIX) {
        memcpy(params, gs.projection[gs.projectionDepth].m, sizeof(float) * 16);
    } else if (pname == GL_TEXTURE_MATRIX) {
        memcpy(params, gs.texture.m, sizeof(float) * 16);
    } else {
        gl3.GetFloatv(pname, params);
    }
}

// Colour
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    ensureInitialised();
    gs.color[0] = r; gs.color[1] = g; gs.color[2] = b; gs.color[3] = a;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) { glColor4f(r, g, b, 1.0f); }

void glNormal3f(GLfloat, GLfloat, GLfloat) {}

// Capabilities
void glEnable(GLenum cap) {
    ensureInitialised();
    if (cap == GL_TEXTURE_2D) gs.texture2D = true;
    else if (cap == GL_ALPHA_TEST) gs.alphaTest = true;
    else if (cap == GL_FOG) gs.fog = true;
    else if (!isIgnoredCap(cap)) gl3.Enable(cap);
}

void glDisable(GLenum cap) {
    ensureInitialised();
    if (cap == GL_TEXTURE_2D) gs.texture2D = false;
    else if (cap == GL_ALPHA_TEST) gs.alphaTest = false;
    else if (cap == GL_FOG) gs.fog = false;
    else if (!isIgnoredCap(cap)) gl3.Disable(cap);
}

void glAlphaFunc(GLenum func, GLclampf ref) {
    ensureInitialised();
    gs.alphaFunc = func;
    gs.alphaRef = ref;
}

// Fog
void glFogf(GLenum pname, GLfloat param) {
    ensureInitialised();
    setFogParam(pname, &param);
}

void glFogfv(GLenum pname, const GLfloat* params) {
    ensureInitialised();
    setFogParam(pname, params);
}

void glFogx(GLenum pname, GLfixed param) {
    ensureInitialised();
    if (pname == GL_FOG_MODE) {
        gs.fogMode = (GLenum)param;  // enums are passed as plain integers
    } else {
        GLfloat f = (GLfloat)param / 65536.0f;
        setFogParam(pname, &f);
    }
}

void glFogi(GLenum pname, GLint param) {
    ensureInitialised();
    if (pname == GL_FOG_MODE) gs.fogMode = (GLenum)param;
    else {
        GLfloat f = (GLfloat)param;
        setFogParam(pname, &f);
    }
}

void glFogxv(GLenum pname, const GLfixed* params) {
    GLfloat f[4];
    for (int i = 0; i < 4; ++i) f[i] = (GLfloat)params[i] / 65536.0f;
    glFogfv(pname, f);
}

// Client arrays
void glEnableClientState(GLenum array) {
    ensureInitialised();
    if (array == GL_VERTEX_ARRAY) gs.arrays[kAttribPosition].enabled = true;
    else if (array == GL_COLOR_ARRAY) gs.arrays[kAttribColor].enabled = true;
    else if (array == GL_TEXTURE_COORD_ARRAY) gs.arrays[kAttribTexCoord].enabled = true;
}

void glDisableClientState(GLenum array) {
    ensureInitialised();
    if (array == GL_VERTEX_ARRAY) gs.arrays[kAttribPosition].enabled = false;
    else if (array == GL_COLOR_ARRAY) gs.arrays[kAttribColor].enabled = false;
    else if (array == GL_TEXTURE_COORD_ARRAY) gs.arrays[kAttribTexCoord].enabled = false;
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    ensureInitialised();
    ClientArray& a = gs.arrays[kAttribPosition];
    a.size = size; a.type = type; a.stride = stride; a.pointer = pointer; a.buffer = gs.arrayBuffer;
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    ensureInitialised();
    ClientArray& a = gs.arrays[kAttribColor];
    a.size = size; a.type = type; a.stride = stride; a.pointer = pointer; a.buffer = gs.arrayBuffer;
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    ensureInitialised();
    ClientArray& a = gs.arrays[kAttribTexCoord];
    a.size = size; a.type = type; a.stride = stride; a.pointer = pointer; a.buffer = gs.arrayBuffer;
}

void glNormalPointer(GLenum, GLsizei, const GLvoid*) {}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (count <= 0) return;
    flushState();

    if (mode == GL_QUADS) {
        GLsizei quads = count / 4;
        if (quads <= 0) return;
        ensureQuadIndices(quads);
        bindArrays(first, quads * 4);
        gl3.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gs.quadIndices);
        gl3.DrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_INT, NULL);
    } else {
        bindArrays(first, count);
        gl3.DrawArrays(mode, 0, count);
    }
}

// Buffers and textures
void glBindBuffer(GLenum target, GLuint buffer) {
    ensureInitialised();
    if (target == GL_ARRAY_BUFFER) gs.arrayBuffer = buffer;
    gl3.BindBuffer(target, buffer);
}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
    ensureInitialised();
    gl3.BufferData(target, size, data, usage);
}

void glGenBuffers(GLsizei n, GLuint* buffers) {
    ensureInitialised();
    gl3.GenBuffers(n, buffers);
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    ensureInitialised();
    gl3.DeleteBuffers(n, buffers);
}

void glBindTexture(GLenum target, GLuint texture) {
    ensureInitialised();
    gl3.BindTexture(target, texture);
}

void glGenTextures(GLsizei n, GLuint* textures) {
    ensureInitialised();
    gl3.GenTextures(n, textures);
}

void glDeleteTextures(GLsizei n, const GLuint* textures) {
    ensureInitialised();
    gl3.DeleteTextures(n, textures);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const GLvoid* pixels) {
    ensureInitialised();
    gl3.TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                     GLsizei height, GLenum format, GLenum type, const GLvoid* pixels) {
    ensureInitialised();
    gl3.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    ensureInitialised();
    gl3.TexParameteri(target, pname, param);
}

void glTexEnvi(GLenum, GLenum, GLint) {}  // MODULATE is the GLES 1 default and the only mode used
void glTexEnvf(GLenum, GLenum, GLfloat) {}
void glTexEnvfv(GLenum, GLenum, const GLfloat*) {}

void glActiveTexture(GLenum texture) {
    ensureInitialised();
    gl3.ActiveTexture(texture);
}

// Framebuffer and state
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { ensureInitialised(); gl3.ClearColor(r, g, b, a); }
void glClearDepthf(GLclampf d) { ensureInitialised(); gl3.ClearDepthf(d); }
void glClear(GLbitfield mask) { ensureInitialised(); gl3.Clear(mask); }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { ensureInitialised(); gl3.ColorMask(r, g, b, a); }
void glDepthMask(GLboolean flag) { ensureInitialised(); gl3.DepthMask(flag); }
void glDepthFunc(GLenum func) { ensureInitialised(); gl3.DepthFunc(func); }
void glDepthRangef(GLclampf n, GLclampf f) { ensureInitialised(); gl3.DepthRangef(n, f); }
void glBlendFunc(GLenum s, GLenum d) { ensureInitialised(); gl3.BlendFunc(s, d); }
void glCullFace(GLenum mode) { ensureInitialised(); gl3.CullFace(mode); }
void glFrontFace(GLenum mode) { ensureInitialised(); gl3.FrontFace(mode); }
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { ensureInitialised(); gl3.Viewport(x, y, w, h); }
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h) { ensureInitialised(); gl3.Scissor(x, y, w, h); }
void glPolygonOffset(GLfloat factor, GLfloat units) { ensureInitialised(); gl3.PolygonOffset(factor, units); }
void glLineWidth(GLfloat w) { ensureInitialised(); gl3.LineWidth(w); }
void glPixelStorei(GLenum pname, GLint param) { ensureInitialised(); gl3.PixelStorei(pname, param); }
void glFlush(void) {}
void glFinish(void) {}
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid* pixels) {
    ensureInitialised();
    gl3.ReadPixels(x, y, w, h, format, type, pixels);
}

GLenum glGetError(void) { ensureInitialised(); return gl3.GetError(); }

const GLubyte* glGetString(GLenum name) { ensureInitialised(); return gl3.GetString(name); }

void glGetIntegerv(GLenum pname, GLint* params) { ensureInitialised(); gl3.GetIntegerv(pname, params); }

// Ignored: hints and modes that GLES 3 dropped (the renderer relies on defaults).
void glHint(GLenum, GLenum) {}
void glShadeModel(GLenum) {}
void glPointSize(GLfloat) {}
void glPolygonMode(GLenum, GLenum) {}

// Lighting is not used by the renderer; these exist so stray calls link.
void glMaterialf(GLenum, GLenum, GLfloat) {}
void glMaterialfv(GLenum, GLenum, const GLfloat*) {}
void glLightf(GLenum, GLenum, GLfloat) {}
void glLightfv(GLenum, GLenum, const GLfloat*) {}
void glLightModelf(GLenum, GLfloat) {}
void glLightModelfv(GLenum, const GLfloat*) {}
void glColorMaterial(GLenum, GLenum) {}

// Display lists were only used by the desktop build path (guarded by OPENGL_ES in
// the engine). Logged so a stray caller shows up instead of silently drawing nothing.
GLuint glGenLists(GLsizei range) {
    static GLuint next = 1;
    GLuint first = next;
    next += (GLuint)range;
    return first;
}
void glNewList(GLuint, GLenum) { SHIM_LOGE("display lists are not supported"); }
void glEndList(void) {}
void glCallList(GLuint) { SHIM_LOGE("display lists are not supported"); }
void glDeleteLists(GLuint, GLsizei) {}

}  // extern "C"
