#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <algorithm>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msimg32.lib")

// ------------------------------------------------------------
// SECOND CLASS PROJECT - SWORD GUARDIAN
// Visual Studio 2010 / Win32 GDI
// Controls: A/D or arrows = move, SPACE = throw sword,
//           P = pause, M = music, ESC = menu
// ------------------------------------------------------------

const int SCREEN_W = 1000;
const int SCREEN_H = 800;
const int TIMER_ID = 1;
const int MAX_LIVES = 10;

enum Scene { MENU, HOW_TO_PLAY, PLAYING, PAUSED, GAME_OVER };

struct Shot {
    float x, y, vx;
    bool alive;
    Shot(float px, float py) : x(px), y(py), vx(650.0f), alive(true) {}
};

struct Enemy {
    float x, y, speed;
    int size;
    bool alive;
    Enemy(float px, float py, float sp, int sz)
        : x(px), y(py), speed(sp), size(sz), alive(true) {}
};

HWND g_window = NULL;
HINSTANCE g_instance = NULL;
Scene g_scene = MENU;
bool g_keys[256] = { false };
bool g_musicOn = true;

HBITMAP g_background = NULL;
HBITMAP g_warrior = NULL;
HBITMAP g_sword = NULL;
HDC g_backDC = NULL;
HBITMAP g_backBitmap = NULL;
HBITMAP g_oldBackBitmap = NULL;

float g_playerX = 145.0f;
float g_playerY = 565.0f;
float g_playerAnim = 0.0f;
int g_lives = MAX_LIVES;
int g_score = 0;
int g_level = 1;
float g_spawnClock = 0.0f;
float g_shotClock = 0.0f;
float g_levelClock = 0.0f;
float g_hitFlash = 0.0f;
float g_menuPulse = 0.0f;
std::vector<Shot> g_shots;
std::vector<Enemy> g_enemies;
DWORD g_lastTick = 0;

float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void DrawText(HDC dc, const wchar_t* text, int x, int y, int w, int h,
              int size, COLORREF color, UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT font = CreateFontW(size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(dc, font);
    RECT r = { x, y, x + w, y + h };
    DrawTextW(dc, text, -1, &r, flags);
    SelectObject(dc, old);
    DeleteObject(font);
}

void Fill(HDC dc, int x, int y, int w, int h, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    RECT r = { x, y, x + w, y + h };
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void DrawPanel(HDC dc, int x, int y, int w, int h, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    HPEN p = CreatePen(PS_SOLID, 1, RGB(110, 190, 225));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, b);
    HPEN oldP = (HPEN)SelectObject(dc, p);
    RoundRect(dc, x, y, x + w, y + h, 22, 22);
    SelectObject(dc, oldB); SelectObject(dc, oldP);
    DeleteObject(b); DeleteObject(p);
}

void DrawBitmapTransparent(HDC dc, HBITMAP bmp, int x, int y, int w, int h) {
    if (!bmp) return;
    HDC src = CreateCompatibleDC(dc);
    HBITMAP old = (HBITMAP)SelectObject(src, bmp);
    BITMAP bm;
    GetObject(bmp, sizeof(bm), &bm);
    TransparentBlt(dc, x, y, w, h, src, 0, 0, bm.bmWidth, bm.bmHeight, RGB(0, 0, 0));
    SelectObject(src, old);
    DeleteDC(src);
}

void DrawBackground(HDC dc) {
    if (g_background) {
        HDC src = CreateCompatibleDC(dc);
        HBITMAP old = (HBITMAP)SelectObject(src, g_background);
        BITMAP bm; GetObject(g_background, sizeof(bm), &bm);
        StretchBlt(dc, 0, 0, SCREEN_W, SCREEN_H, src, 0, 0,
                   bm.bmWidth, bm.bmHeight, SRCCOPY);
        SelectObject(src, old); DeleteDC(src);
    } else {
        for (int y = 0; y < SCREEN_H; ++y) {
            int c = 22 + y * 25 / SCREEN_H;
            HPEN p = CreatePen(PS_SOLID, 1, RGB(c, c + 25, c + 65));
            HPEN old = (HPEN)SelectObject(dc, p);
            MoveToEx(dc, 0, y, NULL); LineTo(dc, SCREEN_W, y);
            SelectObject(dc, old); DeleteObject(p);
        }
    }
    // Keep the supplied background visible; the UI panels provide text contrast.
}

void DrawStarAccent(HDC dc, float t) {
    for (int i = 0; i < 48; ++i) {
        int x = (i * 197 + (int)(t * (8 + i % 5))) % SCREEN_W;
        int y = (i * 113) % SCREEN_H;
        int r = (i % 4 == 0) ? 3 : 2;
        Fill(dc, x, y, r, r, RGB(205, 225, 255));
    }
}

void DrawPlayer(HDC dc) {
    int bob = (int)(sin(g_playerAnim * 8.0f) * 3.0f);
    if (g_hitFlash > 0.0f && ((int)(g_hitFlash * 18) % 2 == 0)) {
        // Brief white flash after taking a hit.
        Fill(dc, (int)g_playerX - 65, (int)g_playerY - 65, 130, 130, RGB(255, 220, 220));
    }
    DrawBitmapTransparent(dc, g_warrior,
        (int)g_playerX - 75, (int)g_playerY - 72 + bob, 150, 134);
}

void DrawEnemy(HDC dc, const Enemy& e) {
    // A stylized shadow creature, drawn with GDI so no extra asset is required.
    HBRUSH b = CreateSolidBrush(RGB(58, 45, 94));
    HPEN p = CreatePen(PS_SOLID, 3, RGB(245, 105, 130));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, b);
    HPEN oldP = (HPEN)SelectObject(dc, p);
    Ellipse(dc, (int)e.x - e.size, (int)e.y - e.size,
            (int)e.x + e.size, (int)e.y + e.size);
    SelectObject(dc, oldB); SelectObject(dc, oldP);
    DeleteObject(b); DeleteObject(p);
    Fill(dc, (int)e.x - 12, (int)e.y - 5, 6, 6, RGB(255, 220, 130));
    Fill(dc, (int)e.x + 6, (int)e.y - 5, 6, 6, RGB(255, 220, 130));
    // Small horns
    POINT horns[3] = {
        { (LONG)e.x - 15, (LONG)e.y - e.size + 8 },
        { (LONG)e.x - 25, (LONG)e.y - e.size - 13 },
        { (LONG)e.x - 4, (LONG)e.y - e.size + 1 }
    };
    HBRUSH hb = CreateSolidBrush(RGB(245, 105, 130));
    HBRUSH oh = (HBRUSH)SelectObject(dc, hb);
    Polygon(dc, horns, 3);
    SelectObject(dc, oh); DeleteObject(hb);
}

void DrawSwordShots(HDC dc) {
    for (size_t i = 0; i < g_shots.size(); ++i) {
        if (g_sword) {
            DrawBitmapTransparent(dc, g_sword,
                (int)g_shots[i].x - 34, (int)g_shots[i].y - 11, 68, 18);
        } else {
            Fill(dc, (int)g_shots[i].x - 18, (int)g_shots[i].y - 3,
                 36, 6, RGB(255, 235, 160));
        }
    }
}

void DrawHUD(HDC dc) {
    DrawPanel(dc, 22, 18, 956, 68, RGB(24, 39, 76));
    wchar_t text[160];
    wsprintfW(text, L"SCORE  %d", g_score);
    DrawText(dc, text, 45, 26, 220, 45, 23, RGB(245, 248, 255), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wsprintfW(text, L"LEVEL  %d", g_level);
    DrawText(dc, text, 405, 26, 180, 45, 23, RGB(125, 220, 255));
    DrawText(dc, L"LIVES", 735, 27, 90, 40, 20, RGB(245, 248, 255));
    for (int i = 0; i < MAX_LIVES; ++i) {
        COLORREF c = (i < g_lives) ? RGB(80, 230, 155) : RGB(75, 83, 105);
        Fill(dc, 820 + i * 14, 40, 10, 18, c);
    }
}

void DrawMenu(HDC dc) {
    DrawPanel(dc, 185, 125, 630, 535, RGB(17, 31, 67));
    DrawText(dc, L"SWORD GUARDIAN", 215, 175, 570, 78, 52, RGB(125, 225, 255));
    DrawText(dc, L"Protect the valley. Defeat the shadow army.", 230, 260, 540, 42, 22, RGB(235, 240, 255));
    DrawBitmapTransparent(dc, g_warrior, 390, 315, 220, 196);
    int pulse = (int)(sin(g_menuPulse * 3.0f) * 12.0f);
    DrawText(dc, L"PRESS ENTER TO START", 260, 535 + pulse, 480, 45, 27, RGB(110, 255, 175));
    DrawText(dc, L"I  -  INSTRUCTIONS       M  -  MUSIC ON/OFF", 250, 590, 500, 35, 17, RGB(245, 220, 135));
    DrawText(dc, L"ESC  -  EXIT", 250, 625, 500, 28, 16, RGB(190, 205, 230));
}

void DrawInstructions(HDC dc) {
    DrawPanel(dc, 155, 145, 690, 500, RGB(17, 31, 67));
    DrawText(dc, L"HOW TO PLAY", 210, 190, 580, 65, 40, RGB(125, 225, 255));
    DrawText(dc, L"A / D or LEFT / RIGHT  -  Move the warrior", 210, 295, 580, 42, 22, RGB(240, 245, 255));
    DrawText(dc, L"SPACE  -  Throw a flying sword", 210, 355, 580, 42, 22, RGB(240, 245, 255));
    DrawText(dc, L"P  -  Pause / resume", 210, 415, 580, 42, 22, RGB(240, 245, 255));
    DrawText(dc, L"M  -  Toggle calm background music", 210, 475, 580, 42, 22, RGB(240, 245, 255));
    DrawText(dc, L"You have 10 lives. Each enemy hit costs one life.", 195, 535, 610, 38, 19, RGB(150, 245, 180));
    DrawText(dc, L"Press ESC to return", 210, 590, 580, 32, 18, RGB(245, 220, 135));
}

void DrawGameOver(HDC dc) {
    DrawPanel(dc, 210, 220, 580, 350, RGB(22, 30, 65));
    DrawText(dc, L"THE VALLEY IS LOST", 245, 275, 510, 65, 39, RGB(255, 125, 145));
    wchar_t score[80];
    wsprintfW(score, L"FINAL SCORE: %d", g_score);
    DrawText(dc, score, 250, 365, 500, 45, 26, RGB(245, 248, 255));
    DrawText(dc, L"ENTER  -  PLAY AGAIN", 250, 440, 500, 40, 22, RGB(115, 255, 175));
    DrawText(dc, L"ESC  -  MAIN MENU", 250, 490, 500, 35, 18, RGB(205, 215, 240));
}

void Render(HDC dc) {
    DrawBackground(dc);
    DrawStarAccent((HDC)dc, g_menuPulse);
    if (g_scene == MENU) { DrawMenu(dc); return; }
    if (g_scene == HOW_TO_PLAY) { DrawInstructions(dc); return; }

    DrawHUD(dc);
    for (size_t i = 0; i < g_enemies.size(); ++i) DrawEnemy(dc, g_enemies[i]);
    DrawSwordShots(dc);
    DrawPlayer(dc);

    if (g_scene == PAUSED) {
        DrawPanel(dc, 300, 315, 400, 150, RGB(18, 30, 66));
        DrawText(dc, L"PAUSED", 325, 340, 350, 55, 40, RGB(130, 225, 255));
        DrawText(dc, L"Press P to continue", 325, 400, 350, 35, 18, RGB(245, 245, 255));
    } else if (g_scene == GAME_OVER) {
        DrawGameOver(dc);
    }
}

void ResetGame() {
    g_playerX = 145.0f;
    g_playerY = 565.0f;
    g_lives = MAX_LIVES;
    g_score = 0;
    g_level = 1;
    g_spawnClock = 0.0f;
    g_shotClock = 0.0f;
    g_levelClock = 0.0f;
    g_hitFlash = 0.0f;
    g_shots.clear();
    g_enemies.clear();
    for (int i = 0; i < 256; ++i) g_keys[i] = false;
    g_scene = PLAYING;
    g_lastTick = GetTickCount();
}

void Update(float dt) {
    g_menuPulse += dt;
    if (g_scene != PLAYING) return;

    g_playerAnim += dt;
    g_shotClock += dt;
    g_spawnClock += dt;
    g_levelClock += dt;
    if (g_hitFlash > 0.0f) g_hitFlash -= dt;

    const float moveSpeed = 360.0f;
    if (g_keys[VK_LEFT] || g_keys['A']) g_playerX -= moveSpeed * dt;
    if (g_keys[VK_RIGHT] || g_keys['D']) g_playerX += moveSpeed * dt;
    if (g_keys[VK_UP] || g_keys['W']) g_playerY -= moveSpeed * dt;
    if (g_keys[VK_DOWN] || g_keys['S']) g_playerY += moveSpeed * dt;
    g_playerX = Clamp(g_playerX, 85.0f, 915.0f);
    g_playerY = Clamp(g_playerY, 170.0f, 690.0f);

    if (g_keys[VK_SPACE] && g_shotClock >= 0.42f) {
        g_shots.push_back(Shot(g_playerX + 65.0f, g_playerY - 5.0f));
        g_shotClock = 0.0f;
    }

    float spawnEvery = std::max(0.62f, 1.65f - (g_level - 1) * 0.12f);
    if (g_spawnClock >= spawnEvery) {
        float y = 180.0f + (float)(rand() % 490);
        float speed = 145.0f + g_level * 22.0f + (float)(rand() % 45);
        int size = 22 + rand() % 9;
        g_enemies.push_back(Enemy(1030.0f, y, speed, size));
        g_spawnClock = 0.0f;
    }

    for (size_t i = 0; i < g_shots.size(); ++i) {
        g_shots[i].x += g_shots[i].vx * dt;
        if (g_shots[i].x > SCREEN_W + 70) g_shots[i].alive = false;
    }

    for (size_t i = 0; i < g_enemies.size(); ++i) {
        g_enemies[i].x -= g_enemies[i].speed * dt;
        if (g_enemies[i].x < -50) {
            g_enemies[i].alive = false;
            --g_lives;
            g_hitFlash = 0.25f;
        }
    }

    // Sword-to-enemy collisions.
    for (size_t e = 0; e < g_enemies.size(); ++e) {
        if (!g_enemies[e].alive) continue;
        for (size_t s = 0; s < g_shots.size(); ++s) {
            if (!g_shots[s].alive) continue;
            float dx = g_enemies[e].x - g_shots[s].x;
            float dy = g_enemies[e].y - g_shots[s].y;
            if (dx * dx + dy * dy < (float)(g_enemies[e].size + 24) * (g_enemies[e].size + 24)) {
                g_enemies[e].alive = false;
                g_shots[s].alive = false;
                g_score += 10;
                break;
            }
        }
    }

    // Enemy-to-player collision.
    for (size_t e = 0; e < g_enemies.size(); ++e) {
        if (!g_enemies[e].alive) continue;
        float dx = g_enemies[e].x - g_playerX;
        float dy = g_enemies[e].y - g_playerY;
        if (dx * dx + dy * dy < 68.0f * 68.0f) {
            g_enemies[e].alive = false;
            --g_lives;
            g_hitFlash = 0.35f;
        }
    }

    g_shots.erase(std::remove_if(g_shots.begin(), g_shots.end(),
        [](const Shot& s) { return !s.alive; }), g_shots.end());
    g_enemies.erase(std::remove_if(g_enemies.begin(), g_enemies.end(),
        [](const Enemy& e) { return !e.alive; }), g_enemies.end());

    if (g_levelClock >= 15.0f) {
        if (g_level < 10) ++g_level;
        g_levelClock = 0.0f;
    }
    if (g_lives <= 0) {
        g_lives = 0;
        g_scene = GAME_OVER;
    }
}

void LoadAssets() {
    g_background = (HBITMAP)LoadImage(NULL, L"assets\\background.bmp",
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    g_warrior = (HBITMAP)LoadImage(NULL, L"assets\\warrior.bmp",
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    g_sword = (HBITMAP)LoadImage(NULL, L"assets\\sword.bmp",
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

void Cleanup() {
    PlaySound(NULL, NULL, 0);
    if (g_backDC) {
        if (g_oldBackBitmap) SelectObject(g_backDC, g_oldBackBitmap);
        DeleteDC(g_backDC);
        g_backDC = NULL;
    }
    if (g_backBitmap) { DeleteObject(g_backBitmap); g_backBitmap = NULL; }
    if (g_background) { DeleteObject(g_background); g_background = NULL; }
    if (g_warrior) { DeleteObject(g_warrior); g_warrior = NULL; }
    if (g_sword) { DeleteObject(g_sword); g_sword = NULL; }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_window = hwnd;
        LoadAssets();
        {
            HDC dc = GetDC(hwnd);
            g_backDC = CreateCompatibleDC(dc);
            g_backBitmap = CreateCompatibleBitmap(dc, SCREEN_W, SCREEN_H);
            ReleaseDC(hwnd, dc);
        }
        g_oldBackBitmap = (HBITMAP)SelectObject(g_backDC, g_backBitmap);
        srand((unsigned int)time(NULL));
        if (g_musicOn)
            PlaySound(L"assets\\calm_theme.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        SetTimer(hwnd, TIMER_ID, 16, NULL);
        g_lastTick = GetTickCount();
        return 0;

    case WM_TIMER:
        if (wp == TIMER_ID) {
            DWORD now = GetTickCount();
            float dt = (now - g_lastTick) / 1000.0f;
            g_lastTick = now;
            dt = Clamp(dt, 0.0f, 0.05f);
            Update(dt);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
        if (wp < 256) g_keys[wp] = true;
        if (wp == VK_ESCAPE) {
            if (g_scene == PLAYING || g_scene == PAUSED || g_scene == HOW_TO_PLAY || g_scene == GAME_OVER)
                g_scene = MENU;
            else
                DestroyWindow(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (g_scene == MENU && wp == VK_RETURN) {
            ResetGame();
        } else if (g_scene == MENU && wp == 'I') {
            g_scene = HOW_TO_PLAY;
        } else if (g_scene == PLAYING && wp == 'P') {
            g_scene = PAUSED;
        } else if (g_scene == PAUSED && wp == 'P') {
            g_scene = PLAYING;
            g_lastTick = GetTickCount();
        } else if (g_scene == GAME_OVER && wp == VK_RETURN) {
            ResetGame();
        } else if (wp == 'M') {
            g_musicOn = !g_musicOn;
            if (g_musicOn)
                PlaySound(L"assets\\calm_theme.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
            else
                PlaySound(NULL, NULL, 0);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_KEYUP:
        if (wp < 256) g_keys[wp] = false;
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (g_backDC) {
                Render(g_backDC);
                BitBlt(dc, 0, 0, SCREEN_W, SCREEN_H, g_backDC, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        Cleanup();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    g_instance = instance;
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SwordGuardianWindow";
    if (!RegisterClassEx(&wc)) return 1;

    RECT r = { 0, 0, SCREEN_W, SCREEN_H };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowEx(0, wc.lpszClassName,
        L"SWORD GUARDIAN - Second Class Project",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
