// ============================================================================
//  ThirdExperiment - a small Win32/GDI game base
//
//  Features
//    * Walking character with 4 directions x 8 animation frames
//    * Transparent sprites (mask technique: AND the mask, then OR the sprite)
//    * Keyboard control (Arrow keys / WASD), Shift to run
//    * Left-click to walk to a point
//    * Right-click or P to pause, F1 to show/hide help, Esc to quit
//    * Calm background music that loops forever (M turns it on / off)
//    * Flicker-free drawing (double buffering)
//    * Fixed-speed movement that does not depend on the computer's speed
//    * Works when the window is resized
//
//  Image files (put them next to WinMain.cpp, or next to the .exe):
//    bg.bmp                        background
//    bgm.wav                       background music
//    go1.bmp go2.bmp go3.bmp go4.bmp   walk sheets: up, down, left, right
//    Each sheet is 480 x 216: 8 frames of 60 x 108.
//    Top half = colour sprite on black, bottom half = mask (white = see-through)
// ============================================================================

#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <math.h>

#pragma comment(lib, "winmm.lib")   // timeBeginPeriod

// ----------------------------------------------------------------------------
//  Settings
// ----------------------------------------------------------------------------
#define WINDOW_WIDTH   1000
#define WINDOW_HEIGHT  800
#define WINDOW_TITLE   L"\u4F5C\u4E1A"      // "作业" (written as escapes so the
                                            // file encoding cannot break it)

const int   FRAME_W       = 60;     // one animation frame (pixels)
const int   FRAME_H       = 108;
const int   FRAME_COUNT   = 8;
const float SPRITE_SCALE  = 1.5f;   // draw the character 1.5x bigger

const float WALK_SPEED    = 180.0f; // pixels per second
const float RUN_SPEED     = 320.0f;
const float WALK_FRAME_TIME = 0.09f; // seconds per animation frame
const float RUN_FRAME_TIME  = 0.05f;

// The order matches the files: go1 = up, go2 = down, go3 = left, go4 = right
enum Direction { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3 };

// ----------------------------------------------------------------------------
//  Game data
// ----------------------------------------------------------------------------
struct Player
{
    float x, y;          // position of the FEET (bottom centre of the sprite)
    Direction dir;       // which way the character faces
    int   frame;         // current animation frame, 0..7
    float animTimer;     // time since the frame last changed
    bool  moving;
    bool  running;
    bool  hasTarget;     // walking towards a mouse click?
    float targetX, targetY;
};

HINSTANCE g_hInst = NULL;
HWND      g_hWnd  = NULL;

HBITMAP g_bgBitmap = NULL;
HBITMAP g_goBitmaps[4] = { NULL, NULL, NULL, NULL };

// Back buffer (we draw here first, then copy to the window in one go)
HDC     g_backDC     = NULL;
HBITMAP g_backBitmap = NULL;
// Background already stretched to the window size (stretched once, not every frame)
HDC     g_bgDC       = NULL;
HBITMAP g_bgScaled   = NULL;
int     g_clientW = 0, g_clientH = 0;

HFONT   g_font = NULL;

Player  g_player;
bool    g_paused   = false;
bool    g_showHelp = true;
int     g_fps      = 0;

// Music
bool    g_musicOpen  = false;   // bgm.wav loaded successfully?
bool    g_musicOn    = true;    // player's choice (M key)

// ----------------------------------------------------------------------------
//  Function declarations
// ----------------------------------------------------------------------------
ATOM    MyWindowsClass(HINSTANCE hInstance);
BOOL    InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void LoadImages();
void ReleaseImages();
void RebuildBuffers(HWND hWnd);
void ReleaseBuffers();

void StartMusic();
void StopMusic();
void SetMusicOn(bool on);
void KeepMusicLooping();

void ResetPlayer();
void Update(float dt);
void RenderFrame();
void Present(HDC hdc);

// ----------------------------------------------------------------------------
//  Entry point + game loop
// ----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine,
                     int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!MyWindowsClass(hInstance))
    {
        MessageBox(NULL, L"Failed to register window class.",
                   L"Error", MB_ICONERROR);
        return 1;
    }

    if (!InitInstance(hInstance, nCmdShow))
    {
        return 1;
    }

    timeBeginPeriod(1);     // make Sleep(1) accurate so we can hold ~60 FPS

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    const float TARGET_DT = 1.0f / 60.0f;
    float fpsTimer = 0.0f;
    int   fpsCount = 0;

    MSG msg = {};
    bool quit = false;

    while (!quit)
    {
        // 1) Handle all pending window messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (quit) break;

        // 2) How much time has passed?
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;

        if (dt < TARGET_DT)
        {
            Sleep(1);
            continue;
        }
        last = now;
        if (dt > 0.1f) dt = 0.1f;   // e.g. after dragging the window

        // 3) Update + draw one frame
        Update(dt);
        RenderFrame();

        HDC hdc = GetDC(g_hWnd);
        Present(hdc);
        ReleaseDC(g_hWnd, hdc);

        // 4) FPS counter (and a once-per-second check that the music still plays)
        fpsCount++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0f)
        {
            g_fps = fpsCount;
            fpsCount = 0;
            fpsTimer -= 1.0f;
            KeepMusicLooping();
        }
    }

    timeEndPeriod(1);
    return (int)msg.wParam;
}

// ----------------------------------------------------------------------------
//  Window creation
// ----------------------------------------------------------------------------
ATOM MyWindowsClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;            // we paint everything ourselves
    wc.lpszClassName = L"gamebase";

    return RegisterClassEx(&wc);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_hInst = hInstance;

    g_hWnd = CreateWindow(
        L"gamebase",
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (g_hWnd == NULL)
    {
        return FALSE;
    }

    g_font = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    LoadImages();
    RebuildBuffers(g_hWnd);
    ResetPlayer();
    StartMusic();

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    return TRUE;
}

// ----------------------------------------------------------------------------
//  Images
// ----------------------------------------------------------------------------

// Tries the current directory first, then the folder that contains the .exe.
// (Visual Studio runs the game from the project folder, but double-clicking
//  the .exe in Debug\ or Release\ would otherwise not find the images.)
static HBITMAP LoadBmp(const wchar_t* fileName)
{
    HBITMAP bmp = (HBITMAP)LoadImage(NULL, fileName, IMAGE_BITMAP, 0, 0,
                                     LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (bmp != NULL)
    {
        return bmp;
    }

    wchar_t path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH) > 0)
    {
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash != NULL)
        {
            *(slash + 1) = 0;
            if (wcslen(path) + wcslen(fileName) < MAX_PATH)
            {
                wcscat_s(path, MAX_PATH, fileName);
                bmp = (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0,
                                         LR_LOADFROMFILE | LR_CREATEDIBSECTION);
            }
        }
    }
    return bmp;
}

void LoadImages()
{
    static const wchar_t* goNames[4] = { L"go1.bmp", L"go2.bmp", L"go3.bmp", L"go4.bmp" };

    wchar_t missing[256] = L"";

    g_bgBitmap = LoadBmp(L"bg.bmp");
    if (g_bgBitmap == NULL)
    {
        wcscat_s(missing, 256, L"  bg.bmp\n");
    }

    for (int i = 0; i < 4; i++)
    {
        g_goBitmaps[i] = LoadBmp(goNames[i]);
        if (g_goBitmaps[i] == NULL)
        {
            wcscat_s(missing, 256, L"  ");
            wcscat_s(missing, 256, goNames[i]);
            wcscat_s(missing, 256, L"\n");
        }
    }

    if (missing[0] != 0)
    {
        wchar_t text[512] = L"Could not load these image files:\n\n";
        wcscat_s(text, 512, missing);
        wcscat_s(text, 512, L"\nPut them next to WinMain.cpp (or next to the .exe).");
        MessageBox(g_hWnd, text, L"Image Loading Error", MB_ICONWARNING);
    }
}

void ReleaseImages()
{
    if (g_bgBitmap != NULL)
    {
        DeleteObject(g_bgBitmap);
        g_bgBitmap = NULL;
    }

    for (int i = 0; i < 4; i++)
    {
        if (g_goBitmaps[i] != NULL)
        {
            DeleteObject(g_goBitmaps[i]);
            g_goBitmaps[i] = NULL;
        }
    }
}

// ----------------------------------------------------------------------------
//  Music (MCI = the simple Windows multimedia command interface)
// ----------------------------------------------------------------------------

// Finds bgm.wav: current directory first, then the folder of the .exe.
static bool FindMusicFile(wchar_t* outPath, int size)
{
    wchar_t full[MAX_PATH];

    if (GetFullPathName(L"bgm.wav", MAX_PATH, full, NULL) > 0 &&
        GetFileAttributes(full) != INVALID_FILE_ATTRIBUTES)
    {
        wcscpy_s(outPath, size, full);
        return true;
    }

    if (GetModuleFileName(NULL, full, MAX_PATH) > 0)
    {
        wchar_t* slash = wcsrchr(full, L'\\');
        if (slash != NULL && wcslen(full) + 8 < MAX_PATH)
        {
            *(slash + 1) = 0;
            wcscat_s(full, MAX_PATH, L"bgm.wav");
            if (GetFileAttributes(full) != INVALID_FILE_ATTRIBUTES)
            {
                wcscpy_s(outPath, size, full);
                return true;
            }
        }
    }
    return false;
}

void StartMusic()
{
    wchar_t path[MAX_PATH];
    if (!FindMusicFile(path, MAX_PATH))
    {
        return;     // no music file: the game simply runs without sound
    }

    wchar_t command[MAX_PATH + 64];
    wsprintf(command, L"open \"%s\" type waveaudio alias bgm", path);

    if (mciSendString(command, NULL, 0, NULL) != 0)
    {
        return;
    }

    g_musicOpen = true;
    // "notify" tells Windows to send our window MM_MCINOTIFY when the song ends
    mciSendString(L"play bgm notify", NULL, 0, g_hWnd);
}

void StopMusic()
{
    if (g_musicOpen)
    {
        mciSendString(L"stop bgm", NULL, 0, NULL);
        mciSendString(L"close bgm", NULL, 0, NULL);
        g_musicOpen = false;
    }
}

// M key: pause / resume the song (it continues where it stopped)
void SetMusicOn(bool on)
{
    g_musicOn = on;
    if (!g_musicOpen)
    {
        return;
    }

    if (on)
        mciSendString(L"play bgm notify", NULL, 0, g_hWnd);
    else
        mciSendString(L"pause bgm", NULL, 0, NULL);
}

// Safety net: if the song stopped for any reason, start it again.
void KeepMusicLooping()
{
    if (!g_musicOpen || !g_musicOn)
    {
        return;
    }

    wchar_t mode[64] = L"";
    mciSendString(L"status bgm mode", mode, 64, NULL);

    if (wcscmp(mode, L"stopped") == 0)
    {
        mciSendString(L"seek bgm to start", NULL, 0, NULL);
        mciSendString(L"play bgm notify", NULL, 0, g_hWnd);
    }
}

// ----------------------------------------------------------------------------
//  Back buffer (re-created whenever the window size changes)
// ----------------------------------------------------------------------------
void ReleaseBuffers()
{
    if (g_backDC)     { DeleteDC(g_backDC);         g_backDC = NULL; }
    if (g_backBitmap) { DeleteObject(g_backBitmap); g_backBitmap = NULL; }
    if (g_bgDC)       { DeleteDC(g_bgDC);           g_bgDC = NULL; }
    if (g_bgScaled)   { DeleteObject(g_bgScaled);   g_bgScaled = NULL; }
}

void RebuildBuffers(HWND hWnd)
{
    ReleaseBuffers();

    RECT rc;
    GetClientRect(hWnd, &rc);
    g_clientW = rc.right - rc.left;
    g_clientH = rc.bottom - rc.top;

    if (g_clientW <= 0 || g_clientH <= 0)
    {
        return;     // minimized
    }

    HDC screen = GetDC(hWnd);

    // The buffer we draw each frame into
    g_backDC = CreateCompatibleDC(screen);
    g_backBitmap = CreateCompatibleBitmap(screen, g_clientW, g_clientH);
    SelectObject(g_backDC, g_backBitmap);
    SelectObject(g_backDC, g_font);
    SetBkMode(g_backDC, TRANSPARENT);

    // The background, stretched to the window size once, with good quality
    g_bgDC = CreateCompatibleDC(screen);
    g_bgScaled = CreateCompatibleBitmap(screen, g_clientW, g_clientH);
    SelectObject(g_bgDC, g_bgScaled);

    if (g_bgBitmap != NULL)
    {
        BITMAP bmp;
        GetObject(g_bgBitmap, sizeof(BITMAP), &bmp);

        HDC src = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(src, g_bgBitmap);

        SetStretchBltMode(g_bgDC, HALFTONE);
        SetBrushOrgEx(g_bgDC, 0, 0, NULL);
        StretchBlt(g_bgDC, 0, 0, g_clientW, g_clientH,
                   src, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);

        SelectObject(src, old);
        DeleteDC(src);
    }
    else
    {
        RECT all = { 0, 0, g_clientW, g_clientH };
        FillRect(g_bgDC, &all, (HBRUSH)(COLOR_WINDOW + 1));
    }

    ReleaseDC(hWnd, screen);
}

// ----------------------------------------------------------------------------
//  Game logic
// ----------------------------------------------------------------------------
static bool KeyDown(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void ResetPlayer()
{
    g_player.x = g_clientW / 2.0f;
    g_player.y = g_clientH - 40.0f;
    g_player.dir = DIR_DOWN;
    g_player.frame = 0;
    g_player.animTimer = 0.0f;
    g_player.moving = false;
    g_player.running = false;
    g_player.hasTarget = false;
    g_player.targetX = g_player.x;
    g_player.targetY = g_player.y;
}

// Keeps a feet position inside the window so the whole sprite stays visible.
static void ClampToWindow(float& x, float& y)
{
    float halfW = FRAME_W * SPRITE_SCALE / 2.0f;
    float height = FRAME_H * SPRITE_SCALE;

    if (x < halfW)                 x = halfW;
    if (x > g_clientW - halfW)     x = g_clientW - halfW;
    if (y < height)                y = height;
    if (y > (float)g_clientH)      y = (float)g_clientH;
}

void Update(float dt)
{
    if (g_paused || g_clientW <= 0 || g_clientH <= 0)
    {
        return;
    }

    Player& p = g_player;

    // Only react to the keyboard while our window is the active one
    bool active = (GetForegroundWindow() == g_hWnd);

    // --- Read the keyboard -------------------------------------------------
    float inX = 0.0f, inY = 0.0f;
    if (active)
    {
        if (KeyDown(VK_LEFT)  || KeyDown('A')) inX -= 1.0f;
        if (KeyDown(VK_RIGHT) || KeyDown('D')) inX += 1.0f;
        if (KeyDown(VK_UP)    || KeyDown('W')) inY -= 1.0f;
        if (KeyDown(VK_DOWN)  || KeyDown('S')) inY += 1.0f;
    }
    p.running = active && KeyDown(VK_SHIFT);

    float speed = p.running ? RUN_SPEED : WALK_SPEED;

    // --- Decide where to go ------------------------------------------------
    float dx = 0.0f, dy = 0.0f;

    if (inX != 0.0f || inY != 0.0f)
    {
        p.hasTarget = false;            // keyboard cancels a mouse click
        dx = inX;
        dy = inY;
    }
    else if (p.hasTarget)
    {
        float toX = p.targetX - p.x;
        float toY = p.targetY - p.y;
        float dist = sqrtf(toX * toX + toY * toY);

        if (dist <= speed * dt)         // close enough: arrive
        {
            p.x = p.targetX;
            p.y = p.targetY;
            p.hasTarget = false;
        }
        else
        {
            dx = toX / dist;
            dy = toY / dist;
        }
    }

    // --- Move --------------------------------------------------------------
    p.moving = (dx != 0.0f || dy != 0.0f);

    if (p.moving)
    {
        // Normalise so diagonal movement is not faster
        float len = sqrtf(dx * dx + dy * dy);
        dx /= len;
        dy /= len;

        p.x += dx * speed * dt;
        p.y += dy * speed * dt;
        ClampToWindow(p.x, p.y);

        // Face the direction we are mostly moving in
        if (fabsf(dx) >= fabsf(dy))
            p.dir = (dx < 0.0f) ? DIR_LEFT : DIR_RIGHT;
        else
            p.dir = (dy < 0.0f) ? DIR_UP : DIR_DOWN;
    }

    // --- Animate -----------------------------------------------------------
    if (p.moving)
    {
        float frameTime = p.running ? RUN_FRAME_TIME : WALK_FRAME_TIME;
        p.animTimer += dt;
        while (p.animTimer >= frameTime)
        {
            p.animTimer -= frameTime;
            p.frame = (p.frame + 1) % FRAME_COUNT;
        }
    }
    else
    {
        p.frame = 0;                    // standing pose
        p.animTimer = 0.0f;
    }
}

// ----------------------------------------------------------------------------
//  Drawing
// ----------------------------------------------------------------------------
static void DrawTextShadow(HDC hdc, int x, int y, const wchar_t* text)
{
    int len = (int)wcslen(text);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOut(hdc, x + 1, y + 1, text, len);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, x, y, text, len);
}

static void DrawPlayer()
{
    const Player& p = g_player;

    int drawW = (int)(FRAME_W * SPRITE_SCALE + 0.5f);
    int drawH = (int)(FRAME_H * SPRITE_SCALE + 0.5f);
    int left  = (int)(p.x - drawW / 2.0f);
    int top   = (int)(p.y - drawH);

    HBITMAP sheet = g_goBitmaps[p.dir];

    if (sheet == NULL)
    {
        // Image missing: draw a placeholder box so the game is still playable
        RECT r = { left, top, left + drawW, top + drawH };
        FillRect(g_backDC, &r, (HBRUSH)GetStockObject(GRAY_BRUSH));
        return;
    }

    HDC memDC = CreateCompatibleDC(g_backDC);
    HGDIOBJ old = SelectObject(memDC, sheet);

    int srcX = p.frame * FRAME_W;

    SetStretchBltMode(g_backDC, COLORONCOLOR);

    // Step 1: AND the mask   -> cuts a black hole in the shape of the character
    StretchBlt(g_backDC, left, top, drawW, drawH,
               memDC, srcX, FRAME_H, FRAME_W, FRAME_H, SRCAND);
    // Step 2: OR the sprite  -> fills the hole with the character's colours
    StretchBlt(g_backDC, left, top, drawW, drawH,
               memDC, srcX, 0, FRAME_W, FRAME_H, SRCPAINT);

    SelectObject(memDC, old);
    DeleteDC(memDC);
}

static void DrawTargetMarker()
{
    if (!g_player.hasTarget)
    {
        return;
    }

    int x = (int)g_player.targetX;
    int y = (int)g_player.targetY;

    HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 60, 60));
    HGDIOBJ oldPen = SelectObject(g_backDC, pen);
    HGDIOBJ oldBrush = SelectObject(g_backDC, GetStockObject(NULL_BRUSH));

    Ellipse(g_backDC, x - 10, y - 5, x + 10, y + 5);
    MoveToEx(g_backDC, x - 5, y, NULL);
    LineTo(g_backDC, x + 6, y);

    SelectObject(g_backDC, oldBrush);
    SelectObject(g_backDC, oldPen);
    DeleteObject(pen);
}

static void DrawHUD()
{
    wchar_t line[128];

    wsprintf(line, L"FPS: %d    Position: %d, %d    %s    Music: %s",
             g_fps, (int)g_player.x, (int)g_player.y,
             g_player.moving ? (g_player.running ? L"Running" : L"Walking")
                             : L"Standing",
             !g_musicOpen ? L"(no file)" : (g_musicOn ? L"on" : L"off"));
    DrawTextShadow(g_backDC, 10, 8, line);

    if (g_showHelp)
    {
        DrawTextShadow(g_backDC, 10, 32, L"Arrow keys / WASD : move");
        DrawTextShadow(g_backDC, 10, 52, L"Shift : run");
        DrawTextShadow(g_backDC, 10, 72, L"Left-click : walk to that point");
        DrawTextShadow(g_backDC, 10, 92, L"Right-click or P : pause / resume");
        DrawTextShadow(g_backDC, 10, 112, L"M : music on / off");
        DrawTextShadow(g_backDC, 10, 132, L"F1 : hide this help      Esc : quit");
    }
    else
    {
        DrawTextShadow(g_backDC, 10, 32, L"F1 : help");
    }

    if (g_paused)
    {
        const wchar_t* text = L"PAUSED";
        SIZE size;
        GetTextExtentPoint32(g_backDC, text, (int)wcslen(text), &size);
        DrawTextShadow(g_backDC, (g_clientW - size.cx) / 2,
                       (g_clientH - size.cy) / 2, text);
    }
}

void RenderFrame()
{
    if (g_backDC == NULL)
    {
        return;
    }

    // Background (already stretched to the right size)
    BitBlt(g_backDC, 0, 0, g_clientW, g_clientH, g_bgDC, 0, 0, SRCCOPY);

    DrawTargetMarker();
    DrawPlayer();
    DrawHUD();
}

// Copy the finished picture to the window in one step (no flicker)
void Present(HDC hdc)
{
    if (g_backDC != NULL)
    {
        BitBlt(hdc, 0, 0, g_clientW, g_clientH, g_backDC, 0, 0, SRCCOPY);
    }
}

// ----------------------------------------------------------------------------
//  Window procedure
// ----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
                         WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        // Needed when the window is uncovered or restored
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RenderFrame();
        Present(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;       // we draw every pixel ourselves; erasing causes flicker

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            RebuildBuffers(hWnd);
            if (g_backDC != NULL)
            {
                float x = g_player.x, y = g_player.y;
                ClampToWindow(x, y);
                g_player.x = x;
                g_player.y = y;
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        // Left-click: walk to the clicked point
        g_player.targetX = (float)GET_X_LPARAM(lParam);
        g_player.targetY = (float)GET_Y_LPARAM(lParam);
        ClampToWindow(g_player.targetX, g_player.targetY);
        g_player.hasTarget = true;
        return 0;

    case WM_RBUTTONDOWN:
        // Right-click: pause / resume (same button as before)
        g_paused = !g_paused;
        return 0;

    case MM_MCINOTIFY:
        // The song reached its end: play it again from the start (endless loop)
        if (wParam == MCI_NOTIFY_SUCCESSFUL && g_musicOpen && g_musicOn)
        {
            mciSendString(L"seek bgm to start", NULL, 0, NULL);
            mciSendString(L"play bgm notify", NULL, 0, hWnd);
        }
        return 0;

    case WM_KEYDOWN:
        if (lParam & 0x40000000)    // ignore key auto-repeat
        {
            return 0;
        }
        switch (wParam)
        {
        case VK_ESCAPE: DestroyWindow(hWnd);     break;
        case 'P':       g_paused = !g_paused;    break;
        case 'M':       SetMusicOn(!g_musicOn);  break;
        case VK_F1:     g_showHelp = !g_showHelp; break;
        }
        return 0;

    case WM_DESTROY:
        StopMusic();
        ReleaseBuffers();
        ReleaseImages();
        if (g_font != NULL)
        {
            DeleteObject(g_font);
            g_font = NULL;
        }
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}
