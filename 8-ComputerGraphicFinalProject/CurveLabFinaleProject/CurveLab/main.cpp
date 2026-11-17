// ============================================================================
//  Curve Lab - Bezier curve with animated de Casteljau construction
//  Win32 + OpenGL (no GLUT).  Target: Visual Studio 2010 (C++03, Win32).
//
//  Modes:   1 = Bezier (degree = points-1, de Casteljau)
//           2 = Catmull-Rom spline (passes through every point)
//           3 = Uniform cubic B-spline (smooth, approximates the points)
//
//  Mouse:   Left click empty space .... add a control point
//           Left drag a point ......... move it
//           Right click a point ....... delete it (minimum 2 points)
//  Keys:    Space  play / pause the animation of t
//           Left / Right  change t by hand        0 / 5  jump to t=0 / t=0.5 (paused)
//           T  show/hide end tangents             D  show/hide construction
//           L  show/hide control polygon          G  grid
//           C  clear to 2 points                  R  reset example
//           Esc exit
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

struct Vec2 { float x, y; };
struct Color { float r, g, b; };

static Vec2 point(float x, float y) { Vec2 v; v.x = x; v.y = y; return v; }
static Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return point(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}
static Color rgb(float r, float g, float b) { Color c; c.r = r; c.g = g; c.b = b; return c; }

// ---------------------------------------------------------------- settings ---
static const int   CLIENT_W = 1000;
static const int   CLIENT_H = 640;
static const int   MAX_PTS  = 24;
static const int   STEPS    = 240;       // samples for Bezier
static const int   SEG_STEPS = 32;       // samples per spline segment
static const float POINT_R  = 8.0f;
static const float PICK_R   = 16.0f;
static const float PI_F     = 3.14159265f;

enum Mode { MODE_BEZIER = 1, MODE_CATMULL = 2, MODE_BSPLINE = 3 };

static HWND   gWindow = NULL;
static HDC    gDC = NULL;
static HGLRC  gRC = NULL;
static GLuint gFontBase = 0;
static int    gWidth = CLIENT_W, gHeight = CLIENT_H;

static Vec2 gPts[MAX_PTS];
static int  gCount = 0;
static int  gMode = MODE_BEZIER;
static int  gSelected = -1, gHover = -1;
static float gT = 0.0f;
static float gDir = 1.0f;
static bool gPlay = true, gShowConstruction = true, gShowPolygon = true, gShowGrid = true;
static bool gShowTangents = false;

static void resetExample() {
    gCount = 5;
    gPts[0] = point(120, 130);
    gPts[1] = point(260, 520);
    gPts[2] = point(500, 120);
    gPts[3] = point(720, 540);
    gPts[4] = point(880, 220);
}

// -------------------------------------------------------------- curve math ---
// Bezier by de Casteljau.
static Vec2 bezierPoint(float t) {
    Vec2 tmp[MAX_PTS];
    for (int i = 0; i < gCount; ++i) tmp[i] = gPts[i];
    for (int k = 1; k < gCount; ++k)
        for (int i = 0; i < gCount - k; ++i)
            tmp[i] = lerp(tmp[i], tmp[i + 1], t);
    return tmp[0];
}

static Vec2 catmullSegment(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    Vec2 r;
    r.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    r.y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                  (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    return r;
}

static Vec2 bsplineSegment(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    float b0 = (1.0f - t) * (1.0f - t) * (1.0f - t) / 6.0f;
    float b1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
    float b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
    float b3 = t3 / 6.0f;
    return point(b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
                 b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Builds the polyline for the current mode. Returns number of vertices.
static int buildCurve(Vec2* out, int maxOut) {
    int n = 0;
    if (gCount < 2) return 0;
    if (gMode == MODE_BEZIER) {
        for (int i = 0; i <= STEPS && n < maxOut; ++i)
            out[n++] = bezierPoint((float)i / (float)STEPS);
    } else if (gMode == MODE_CATMULL) {
        for (int s = 0; s < gCount - 1; ++s) {
            Vec2 p0 = gPts[clampi(s - 1, 0, gCount - 1)];
            Vec2 p1 = gPts[s];
            Vec2 p2 = gPts[s + 1];
            Vec2 p3 = gPts[clampi(s + 2, 0, gCount - 1)];
            for (int i = 0; i < SEG_STEPS && n < maxOut; ++i)
                out[n++] = catmullSegment(p0, p1, p2, p3, (float)i / (float)SEG_STEPS);
        }
        if (n < maxOut) out[n++] = gPts[gCount - 1];
    } else {
        // padded control list: P0,P0,P0,P1..Pn-1,Pn-1,Pn-1  -> curve touches both ends
        Vec2 q[MAX_PTS + 4];
        int m = 0;
        q[m++] = gPts[0]; q[m++] = gPts[0];
        for (int i = 0; i < gCount; ++i) q[m++] = gPts[i];
        q[m++] = gPts[gCount - 1]; q[m++] = gPts[gCount - 1];
        for (int s = 0; s + 3 < m; ++s)
            for (int i = 0; i <= SEG_STEPS && n < maxOut; ++i)
                out[n++] = bsplineSegment(q[s], q[s + 1], q[s + 2], q[s + 3],
                                          (float)i / (float)SEG_STEPS);
    }
    return n;
}

// -------------------------------------------------------------- GL helpers ---
static void fillCircle(Vec2 p, float r, Color c) {
    glColor3f(c.r, c.g, c.b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(p.x, p.y);
    for (int i = 0; i <= 28; ++i) {
        float a = 2.0f * PI_F * (float)i / 28.0f;
        glVertex2f(p.x + r * (float)cos(a), p.y + r * (float)sin(a));
    }
    glEnd();
}

static void ring(Vec2 p, float r, Color c) {
    glColor3f(c.r, c.g, c.b);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 28; ++i) {
        float a = 2.0f * PI_F * (float)i / 28.0f;
        glVertex2f(p.x + r * (float)cos(a), p.y + r * (float)sin(a));
    }
    glEnd();
}

static void initFont() {
    HFONT font = CreateFontA(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, FF_DONTCARE, "Arial");
    if (!font) return;
    HGDIOBJ old = SelectObject(gDC, font);
    gFontBase = glGenLists(128);
    if (gFontBase && !wglUseFontBitmapsA(gDC, 0, 128, gFontBase)) {
        glDeleteLists(gFontBase, 128);
        gFontBase = 0;
    }
    SelectObject(gDC, old);
    DeleteObject(font);
}

static void text(float x, float y, const char* s) {
    if (!gFontBase || !s) return;
    glRasterPos2f(x, y);
    glListBase(gFontBase);
    glCallLists((GLsizei)lstrlenA(s), GL_UNSIGNED_BYTE, (const GLubyte*)s);
}

static void reshape(int w, int h) {
    gWidth = w > 0 ? w : 1;
    gHeight = h > 0 ? h : 1;
    if (!gRC) return;
    glViewport(0, 0, gWidth, gHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)gWidth, 0.0, (double)gHeight, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void drawGrid() {
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int x = 0; x < gWidth; x += 50) {
        if (x % 250 == 0) glColor3f(0.16f, 0.20f, 0.28f); else glColor3f(0.10f, 0.13f, 0.19f);
        glVertex2f((float)x, 0.0f); glVertex2f((float)x, (float)gHeight);
    }
    for (int y = 0; y < gHeight; y += 50) {
        if (y % 250 == 0) glColor3f(0.16f, 0.20f, 0.28f); else glColor3f(0.10f, 0.13f, 0.19f);
        glVertex2f(0.0f, (float)y); glVertex2f((float)gWidth, (float)y);
    }
    glEnd();
}

// de Casteljau construction at parameter t (Bezier mode).
static void drawConstruction(float t) {
    static const Color levelColors[6] = {
        { 1.00f, 0.62f, 0.25f }, { 0.95f, 0.85f, 0.30f }, { 0.55f, 0.90f, 0.40f },
        { 0.35f, 0.85f, 0.95f }, { 0.65f, 0.55f, 1.00f }, { 1.00f, 0.50f, 0.80f }
    };
    Vec2 lvl[MAX_PTS][MAX_PTS];
    for (int i = 0; i < gCount; ++i) lvl[0][i] = gPts[i];
    for (int k = 1; k < gCount; ++k)
        for (int i = 0; i < gCount - k; ++i)
            lvl[k][i] = lerp(lvl[k - 1][i], lvl[k - 1][i + 1], t);

    glLineWidth(1.8f);
    for (int k = 1; k < gCount - 1; ++k) {
        Color c = levelColors[(k - 1) % 6];
        glColor3f(c.r, c.g, c.b);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < gCount - k; ++i) glVertex2f(lvl[k][i].x, lvl[k][i].y);
        glEnd();
    }
    for (int k = 1; k < gCount - 1; ++k) {
        Color c = levelColors[(k - 1) % 6];
        for (int i = 0; i < gCount - k; ++i) fillCircle(lvl[k][i], 4.5f, c);
    }
    // final point
    Vec2 f = lvl[gCount - 1][0];
    fillCircle(f, 9.0f, rgb(1.0f, 0.30f, 0.35f));
    ring(f, 13.0f, rgb(1.0f, 1.0f, 1.0f));
}

static void arrowHead(Vec2 from, Vec2 to, float size) {
    float dx = to.x - from.x, dy = to.y - from.y;
    float len = (float)sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    dx /= len; dy /= len;
    Vec2 base = point(to.x - dx * size, to.y - dy * size);
    glBegin(GL_TRIANGLES);
    glVertex2f(to.x, to.y);
    glVertex2f(base.x - dy * size * 0.5f, base.y + dx * size * 0.5f);
    glVertex2f(base.x + dy * size * 0.5f, base.y - dx * size * 0.5f);
    glEnd();
}

// End tangents of the Bezier curve: B'(0) ~ P1-P0 and B'(1) ~ Pn-Pn-1 (scaled for display).
static void drawTangents() {
    if (gCount < 2) return;
    Vec2 s = gPts[0];
    Vec2 e = gPts[gCount - 1];
    Vec2 a0 = point(s.x + (gPts[1].x - s.x) * 0.5f, s.y + (gPts[1].y - s.y) * 0.5f);
    Vec2 prev = gPts[gCount - 2];
    Vec2 a1 = point(e.x + (e.x - prev.x) * 0.5f, e.y + (e.y - prev.y) * 0.5f);
    glColor3f(1.0f, 0.62f, 0.25f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(s.x, s.y); glVertex2f(a0.x, a0.y);
    glVertex2f(e.x, e.y); glVertex2f(a1.x, a1.y);
    glEnd();
    arrowHead(s, a0, 16.0f);
    arrowHead(e, a1, 16.0f);
    text(a0.x + 12.0f, a0.y - 4.0f, "B'(0)");
    text(a1.x + 12.0f, a1.y - 4.0f, "B'(1)");
}

static const char* modeName() {
    if (gMode == MODE_BEZIER) return "1  Bezier curve (de Casteljau)";
    if (gMode == MODE_CATMULL) return "2  Catmull-Rom spline (interpolating)";
    return "3  Uniform cubic B-spline (approximating)";
}

static void draw() {
    if (!gRC) return;
    char buf[200];
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (gShowGrid) drawGrid();

    // control polygon
    if (gShowPolygon && gCount >= 2) {
        glLineWidth(1.5f);
        glColor3f(0.40f, 0.47f, 0.60f);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x0F0F);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < gCount; ++i) glVertex2f(gPts[i].x, gPts[i].y);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
    }

    // the curve
    Vec2 curve[(MAX_PTS + 4) * (SEG_STEPS + 1) + STEPS + 8];
    int n = buildCurve(curve, (int)(sizeof(curve) / sizeof(curve[0])));
    if (n >= 2) {
        glLineWidth(3.5f);
        glColor3f(0.29f, 0.94f, 0.69f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < n; ++i) glVertex2f(curve[i].x, curve[i].y);
        glEnd();
    }

    if (gMode == MODE_BEZIER && gCount >= 2) {
        // bright trail up to t
        int upto = (int)(gT * (float)STEPS);
        if (upto >= 1) {
            glLineWidth(5.0f);
            glColor3f(1.0f, 0.30f, 0.35f);
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= upto && i < n; ++i) glVertex2f(curve[i].x, curve[i].y);
            glEnd();
        }
        if (gShowConstruction) drawConstruction(gT);
        if (gShowTangents) drawTangents();
    }

    // control points
    for (int i = 0; i < gCount; ++i)
        fillCircle(gPts[i], POINT_R, rgb(0.30f, 0.69f, 1.0f));
    int active = gSelected >= 0 ? gSelected : gHover;
    if (active >= 0 && active < gCount) {
        glLineWidth(2.0f);
        ring(gPts[active], POINT_R + 4.0f, rgb(1.0f, 1.0f, 1.0f));
    }
    glColor3f(0.92f, 0.95f, 0.99f);
    for (int i = 0; i < gCount; ++i) {
        sprintf(buf, "P%d", i);
        text(gPts[i].x + 12.0f, gPts[i].y - 18.0f, buf);
    }

    // HUD
    glColor3f(0.95f, 0.97f, 1.0f);
    text(20.0f, (float)gHeight - 28.0f, "Curve Lab");
    glColor3f(0.55f, 0.85f, 0.75f);
    text(20.0f, (float)gHeight - 50.0f, modeName());
    glColor3f(0.80f, 0.86f, 0.95f);
    if (gMode == MODE_BEZIER)
        sprintf(buf, "control points: %d   degree: %d   t = %.3f   %s",
                gCount, gCount - 1, gT, gPlay ? "(playing)" : "(paused)");
    else
        sprintf(buf, "control points: %d", gCount);
    text(20.0f, (float)gHeight - 72.0f, buf);

    glColor3f(0.68f, 0.75f, 0.84f);
    text(20.0f, 32.0f, "Click: add point | Drag: move | Right-click: delete | 1/2/3: curve type | Space: play/pause");
    text(20.0f, 10.0f, "Left/Right: t | 0 / 5: jump to t=0 / 0.5 | T tangents | D construction | L polygon | G grid | C clear | R reset | Esc");

    SwapBuffers(gDC);
}

// ------------------------------------------------------------------ mouse ---
static float dist2(Vec2 a, Vec2 b) { float dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy; }

static Vec2 mousePt(LPARAM lp) {
    return point((float)GET_X_LPARAM(lp), (float)(gHeight - GET_Y_LPARAM(lp)));
}

static int pick(Vec2 m) {
    int best = -1;
    float closest = PICK_R * PICK_R;
    for (int i = 0; i < gCount; ++i) {
        float d = dist2(m, gPts[i]);
        if (d <= closest) { closest = d; best = i; }
    }
    return best;
}

static void deletePoint(int idx) {
    for (int i = idx; i < gCount - 1; ++i) gPts[i] = gPts[i + 1];
    --gCount;
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;

        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) reshape(LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            draw();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER:
            if (gPlay && gMode == MODE_BEZIER) {
                gT += 0.004f * gDir;
                if (gT >= 1.0f) { gT = 1.0f; gDir = -1.0f; }
                if (gT <= 0.0f) { gT = 0.0f; gDir = 1.0f; }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursor(NULL, (gSelected >= 0 || gHover >= 0) ? IDC_HAND : IDC_CROSS));
                return TRUE;
            }
            break;

        case WM_LBUTTONDOWN: {
            Vec2 m = mousePt(lp);
            gSelected = pick(m);
            if (gSelected < 0 && gCount < MAX_PTS) {
                gPts[gCount] = m;
                gSelected = gCount++;
            }
            if (gSelected >= 0) SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_RBUTTONDOWN: {
            int idx = pick(mousePt(lp));
            if (idx >= 0 && gCount > 2) {
                deletePoint(idx);
                gHover = -1;
                gSelected = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            Vec2 m = mousePt(lp);
            if (gSelected >= 0) {
                gPts[gSelected] = m;
                InvalidateRect(hwnd, NULL, FALSE);
            } else {
                int h = pick(m);
                if (h != gHover) { gHover = h; InvalidateRect(hwnd, NULL, FALSE); }
            }
            return 0;
        }

        case WM_LBUTTONUP:
            gSelected = -1;
            if (GetCapture() == hwnd) ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_CAPTURECHANGED:
            gSelected = -1;
            return 0;

        case WM_KEYDOWN:
            switch (wp) {
                case '1': gMode = MODE_BEZIER;  break;
                case '2': gMode = MODE_CATMULL; break;
                case '3': gMode = MODE_BSPLINE; break;
                case VK_SPACE: gPlay = !gPlay; break;
                case VK_LEFT:  gPlay = false; gT -= 0.01f; if (gT < 0.0f) gT = 0.0f; break;
                case VK_RIGHT: gPlay = false; gT += 0.01f; if (gT > 1.0f) gT = 1.0f; break;
                case '0': gPlay = false; gT = 0.0f; gDir = 1.0f; break;
                case '5': gPlay = false; gT = 0.5f; break;
                case 'T': gShowTangents = !gShowTangents; break;
                case 'D': gShowConstruction = !gShowConstruction; break;
                case 'L': gShowPolygon = !gShowPolygon; break;
                case 'G': gShowGrid = !gShowGrid; break;
                case 'C': gCount = 2; gPts[0] = point(200, 200); gPts[1] = point(800, 440); gHover = -1; break;
                case 'R': resetExample(); gHover = -1; break;
                case VK_ESCAPE: DestroyWindow(hwnd); return 0;
                default: return 0;
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void cleanup() {
    if (gRC) {
        wglMakeCurrent(gDC, gRC);
        if (gFontBase) glDeleteLists(gFontBase, 128);
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(gRC);
        gRC = NULL;
    }
    if (gDC && gWindow) { ReleaseDC(gWindow, gDC); gDC = NULL; }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int showCmd) {
    resetExample();

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.lpszClassName = "CurveLabWindow";
    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Cannot register window class.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rc = { 0, 0, CLIENT_W, CLIENT_H };
    AdjustWindowRect(&rc, style, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2; if (x < 0) x = 0;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2; if (y < 0) y = 0;

    gWindow = CreateWindowExA(0, wc.lpszClassName, "Curve Lab - Bezier / de Casteljau",
                              style, x, y, w, h, NULL, NULL, inst, NULL);
    if (!gWindow) {
        MessageBoxA(NULL, "Cannot create window.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    gDC = GetDC(gWindow);

    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(gDC, &pfd);
    if (!fmt || !SetPixelFormat(gDC, fmt, &pfd)) {
        MessageBoxA(gWindow, "Cannot set OpenGL pixel format.", "Error", MB_OK | MB_ICONERROR);
        cleanup(); DestroyWindow(gWindow); return 1;
    }
    gRC = wglCreateContext(gDC);
    if (!gRC || !wglMakeCurrent(gDC, gRC)) {
        MessageBoxA(gWindow, "Cannot initialize OpenGL.", "Error", MB_OK | MB_ICONERROR);
        cleanup(); DestroyWindow(gWindow); return 1;
    }

    glClearColor(0.065f, 0.08f, 0.12f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    RECT cr;
    GetClientRect(gWindow, &cr);
    reshape(cr.right - cr.left, cr.bottom - cr.top);
    initFont();
    SetTimer(gWindow, 1, 16, NULL);

    ShowWindow(gWindow, showCmd);
    UpdateWindow(gWindow);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    cleanup();
    return (int)msg.wParam;
}
