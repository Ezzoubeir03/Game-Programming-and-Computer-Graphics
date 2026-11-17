// Sword Guardian - original user graphics preserved
// Visual Studio 2010 / Win32 API / GDI
#include <windows.h>
#include <mmsystem.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msimg32.lib")

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 800
#define TIMER_ID 1
#define MAX_BULLETS 30
#define MAX_ENEMIES 12
#define MAX_LIVES 10

struct SwordBullet { float x, y; bool exist; };
struct Enemy { float x, y, speed; bool exist; };

HINSTANCE g_instance = NULL;
HWND g_window = NULL;
HDC g_screenDC = NULL;
HDC g_backDC = NULL;
HBITMAP g_backBitmap = NULL;
HBITMAP g_oldBackBitmap = NULL;
HBITMAP g_background = NULL;
HBITMAP g_warrior = NULL;
HBITMAP g_sword = NULL;

SwordBullet g_bullets[MAX_BULLETS];
Enemy g_enemies[MAX_ENEMIES];

float g_playerX = 90.0f;
float g_playerY = 500.0f;
float g_backgroundOffset = 0.0f;
float g_spawnClock = 0.0f;
float g_shotClock = 0.0f;
float g_levelClock = 0.0f;
DWORD g_lastTick = 0;
int g_score = 0;
int g_lives = MAX_LIVES;
int g_level = 1;
bool g_musicOn = true;
bool g_keys[256] = { false };

static float ClampFloat(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static HBITMAP LoadBitmapFile(const wchar_t* path)
{
    return (HBITMAP)LoadImageW(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

static void DrawTransparentBitmap(HDC destination, HBITMAP bitmap,
                                  int x, int y, int width, int height)
{
    if (bitmap == NULL) return;
    HDC source = CreateCompatibleDC(destination);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(source, bitmap);
    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    TransparentBlt(destination, x, y, width, height,
                   source, 0, 0, info.bmWidth, info.bmHeight,
                   RGB(0, 0, 0));
    SelectObject(source, oldBitmap);
    DeleteDC(source);
}

static void DrawBackground(HDC dc)
{
    if (g_background == NULL)
    {
        PatBlt(dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACKNESS);
        return;
    }

    HDC source = CreateCompatibleDC(dc);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(source, g_background);
    BITMAP info;
    GetObject(g_background, sizeof(info), &info);

    // Tile the exact supplied cloud image horizontally; do not replace or recolor it.
    int tileWidth = WINDOW_WIDTH;
    int tileHeight = WINDOW_HEIGHT;
    int offset = (int)g_backgroundOffset;
    int firstWidth = tileWidth - offset;
    if (firstWidth > 0)
        StretchBlt(dc, 0, 0, firstWidth, tileHeight,
                   source, offset * info.bmWidth / tileWidth, 0,
                   firstWidth * info.bmWidth / tileWidth, info.bmHeight, SRCCOPY);
    if (offset > 0)
        StretchBlt(dc, firstWidth, 0, offset, tileHeight,
                   source, 0, 0, offset * info.bmWidth / tileWidth,
                   info.bmHeight, SRCCOPY);

    SelectObject(source, oldBitmap);
    DeleteDC(source);
}

static void DrawTextAt(HDC dc, const wchar_t* text, int x, int y, COLORREF color, int size)
{
    HFONT font = CreateFontW(size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text, lstrlenW(text));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

static void ResetGame()
{
    int i;
    g_playerX = 90.0f;
    g_playerY = 500.0f;
    g_score = 0;
    g_lives = MAX_LIVES;
    g_level = 1;
    g_spawnClock = 0.0f;
    g_shotClock = 0.0f;
    g_levelClock = 0.0f;
    for (i = 0; i < MAX_BULLETS; ++i) g_bullets[i].exist = false;
    for (i = 0; i < MAX_ENEMIES; ++i) g_enemies[i].exist = false;
}

static void FireSword()
{
    int i;
    if (g_shotClock < 0.22f) return;
    for (i = 0; i < MAX_BULLETS; ++i)
    {
        if (!g_bullets[i].exist)
        {
            g_bullets[i].x = g_playerX + 125.0f;
            g_bullets[i].y = g_playerY + 78.0f;
            g_bullets[i].exist = true;
            g_shotClock = 0.0f;
            return;
        }
    }
}

static void UpdateGame(float dt)
{
    int i, j;
    if (g_keys[VK_LEFT] || g_keys['A']) g_playerX -= 330.0f * dt;
    if (g_keys[VK_RIGHT] || g_keys['D']) g_playerX += 330.0f * dt;
    if (g_keys[VK_UP] || g_keys['W']) g_playerY -= 330.0f * dt;
    if (g_keys[VK_DOWN] || g_keys['S']) g_playerY += 330.0f * dt;
    if (g_keys[VK_SPACE]) FireSword();

    g_playerX = ClampFloat(g_playerX, 0.0f, WINDOW_WIDTH - 190.0f);
    g_playerY = ClampFloat(g_playerY, 105.0f, WINDOW_HEIGHT - 175.0f);
    g_backgroundOffset += 24.0f * dt;
    if (g_backgroundOffset >= WINDOW_WIDTH) g_backgroundOffset -= WINDOW_WIDTH;
    g_spawnClock += dt;
    g_shotClock += dt;
    g_levelClock += dt;

    if (g_levelClock >= 20.0f)
    {
        g_levelClock = 0.0f;
        if (g_level < 8) ++g_level;
    }

    if (g_spawnClock >= (1.65f - g_level * 0.10f))
    {
        for (i = 0; i < MAX_ENEMIES; ++i)
        {
            if (!g_enemies[i].exist)
            {
                g_enemies[i].x = WINDOW_WIDTH + 25.0f;
                g_enemies[i].y = 145.0f + (float)(rand() % (WINDOW_HEIGHT - 210));
                g_enemies[i].speed = 95.0f + g_level * 17.0f;
                g_enemies[i].exist = true;
                break;
            }
        }
        g_spawnClock = 0.0f;
    }

    for (i = 0; i < MAX_BULLETS; ++i)
    {
        if (g_bullets[i].exist)
        {
            g_bullets[i].x += 620.0f * dt;
            if (g_bullets[i].x > WINDOW_WIDTH + 100) g_bullets[i].exist = false;
        }
    }

    for (i = 0; i < MAX_ENEMIES; ++i)
    {
        if (g_enemies[i].exist)
        {
            g_enemies[i].x -= g_enemies[i].speed * dt;
            if (g_enemies[i].x < -35.0f)
            {
                g_enemies[i].exist = false;
                --g_lives;
            }
        }
    }

    // Sword/enemy collision.
    for (i = 0; i < MAX_BULLETS; ++i)
    {
        if (!g_bullets[i].exist) continue;
        for (j = 0; j < MAX_ENEMIES; ++j)
        {
            if (!g_enemies[j].exist) continue;
            if (fabs(g_bullets[i].x - g_enemies[j].x) < 42.0f &&
                fabs(g_bullets[i].y - g_enemies[j].y) < 34.0f)
            {
                g_bullets[i].exist = false;
                g_enemies[j].exist = false;
                g_score += 10;
                break;
            }
        }
    }

    // Enemy/player collision.
    for (i = 0; i < MAX_ENEMIES; ++i)
    {
        if (!g_enemies[i].exist) continue;
        if (fabs(g_enemies[i].x - (g_playerX + 85.0f)) < 65.0f &&
            fabs(g_enemies[i].y - (g_playerY + 85.0f)) < 65.0f)
        {
            g_enemies[i].exist = false;
            --g_lives;
        }
    }
    if (g_lives < 0) g_lives = 0;
}

static void DrawScene(HDC dc)
{
    int i;
    DrawBackground(dc);

    // Keep the supplied warrior bitmap and its original proportions.
    DrawTransparentBitmap(dc, g_warrior,
        (int)g_playerX, (int)g_playerY, 190, 170);

    // Draw the exact supplied sword bitmap for every active projectile.
    for (i = 0; i < MAX_BULLETS; ++i)
    {
        if (g_bullets[i].exist)
            DrawTransparentBitmap(dc, g_sword,
                (int)g_bullets[i].x, (int)g_bullets[i].y, 100, 26);
    }

    // Simple GDI enemies; no replacement player/background artwork is used.
    for (i = 0; i < MAX_ENEMIES; ++i)
    {
        if (g_enemies[i].exist)
        {
            HBRUSH brush = CreateSolidBrush(RGB(95, 45, 75));
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 225, 190));
            HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
            HPEN oldPen = (HPEN)SelectObject(dc, pen);
            Ellipse(dc, (int)g_enemies[i].x - 18, (int)g_enemies[i].y - 18,
                    (int)g_enemies[i].x + 18, (int)g_enemies[i].y + 18);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }
    }

    // Small readable status text; background and sprites remain unobstructed.
    wchar_t status[160];
    wsprintfW(status, L"SCORE: %d     LIVES: %d/%d     LEVEL: %d",
              g_score, g_lives, MAX_LIVES, g_level);
    DrawTextAt(dc, status, 18, 14, RGB(25, 45, 95), 23);
    DrawTextAt(dc, L"MOVE: ARROWS / W A S D     THROW SWORD: SPACE OR LEFT CLICK     M: MUSIC",
               18, WINDOW_HEIGHT - 34, RGB(25, 45, 95), 17);
    if (g_lives <= 0)
    {
        DrawTextAt(dc, L"GAME OVER - PRESS ENTER TO PLAY AGAIN", 245, 365,
                   RGB(125, 20, 45), 34);
    }
}

static void Render()
{
    if (g_backDC == NULL) return;
    DrawScene(g_backDC);
    HDC dc = GetDC(g_window);
    BitBlt(dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, g_backDC, 0, 0, SRCCOPY);
    ReleaseDC(g_window, dc);
}

static void Cleanup()
{
    KillTimer(g_window, TIMER_ID);
    PlaySound(NULL, NULL, 0);
    if (g_backDC)
    {
        if (g_oldBackBitmap) SelectObject(g_backDC, g_oldBackBitmap);
        DeleteDC(g_backDC);
        g_backDC = NULL;
    }
    if (g_backBitmap) DeleteObject(g_backBitmap);
    if (g_background) DeleteObject(g_background);
    if (g_warrior) DeleteObject(g_warrior);
    if (g_sword) DeleteObject(g_sword);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        g_screenDC = GetDC(hwnd);
        g_backDC = CreateCompatibleDC(g_screenDC);
        g_backBitmap = CreateCompatibleBitmap(g_screenDC, WINDOW_WIDTH, WINDOW_HEIGHT);
        g_oldBackBitmap = (HBITMAP)SelectObject(g_backDC, g_backBitmap);
        ReleaseDC(hwnd, g_screenDC);
        g_screenDC = NULL;
        SetTimer(hwnd, TIMER_ID, 16, NULL);
        g_lastTick = GetTickCount();
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
            DWORD now = GetTickCount();
            float dt = (float)(now - g_lastTick) / 1000.0f;
            g_lastTick = now;
            if (dt > 0.05f) dt = 0.05f;
            if (g_lives > 0) UpdateGame(dt);
            Render();
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_backDC) BitBlt(dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, g_backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam < 256) g_keys[wParam] = true;
        if (wParam == VK_RETURN && g_lives <= 0) ResetGame();
        if (wParam == 'M')
        {
            g_musicOn = !g_musicOn;
            if (g_musicOn) PlaySoundW(L"assets\\calm_theme.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
            else PlaySound(NULL, NULL, 0);
        }
        return 0;

    case WM_KEYUP:
        if (wParam < 256) g_keys[wParam] = false;
        return 0;

    case WM_LBUTTONDOWN:
        if (g_lives > 0) FireSword();
        return 0;

    case WM_MOUSEMOVE:
        // Preserve keyboard movement; mouse movement is not used to replace the character controls.
        return 0;

    case WM_DESTROY:
        Cleanup();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SwordGuardianOriginalWindow";
    if (!RegisterClassExW(&wc)) return 0;

    RECT r = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_window = CreateWindowW(wc.lpszClassName, L"Sword Guardian - Original Graphics",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top, NULL, NULL, instance, NULL);
    if (!g_window) return 0;

    g_instance = instance;
    g_background = LoadBitmapFile(L"assets\\background.bmp");
    g_warrior = LoadBitmapFile(L"assets\\warrior.bmp");
    g_sword = LoadBitmapFile(L"assets\\sword.bmp");
    if (!g_background || !g_warrior || !g_sword)
    {
        MessageBoxW(NULL,
            L"One or more original BMP files could not be loaded. Check the assets folder and Working Directory.",
            L"Missing game graphics", MB_OK | MB_ICONERROR);
    }

    srand((unsigned int)time(NULL));
    ZeroMemory(g_bullets, sizeof(g_bullets));
    ZeroMemory(g_enemies, sizeof(g_enemies));
    ShowWindow(g_window, showCommand);
    UpdateWindow(g_window);
    PlaySoundW(L"assets\\calm_theme.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
