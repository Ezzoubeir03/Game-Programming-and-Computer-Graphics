
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <time.h>

using namespace std;

// ==========================================
// 1. GAME CONFIGURATION
// ==========================================

#define WIDTH 800
#define HEIGHT 650
#define GAME_TIMER 1

const int MAX_HEALTH = 10;

// ==========================================
// 2. GAME STATES
// ==========================================

enum GameState {
    MENU,
    INSTRUCTIONS,
    PLAYING,
    GAME_OVER
};

GameState g_state = MENU;

// ==========================================
// 3. GAME STRUCTURES
// ==========================================

struct Bullet {
    float x;
    float y;

    Bullet(float px, float py) {
        x = px;
        y = py;
    }
};

struct Enemy {
    float x;
    float y;
    float speed;

    Enemy(float px, float py, float ps) {
        x = px;
        y = py;
        speed = ps;
    }
};

// ==========================================
// 4. GLOBAL VARIABLES
// ==========================================

HWND g_hwnd = NULL;

float g_playerX = WIDTH / 2.0f;
float g_playerY = HEIGHT - 65.0f;

int g_score = 0;
int g_health = MAX_HEALTH;
int g_level = 1;

bool g_keys[256] = { false };

vector<Bullet> g_bullets;
vector<Enemy> g_enemies;

DWORD g_lastShot = 0;
DWORD g_lastSpawn = 0;
DWORD g_lastLevel = 0;

// ==========================================
// 5. HELPER FUNCTIONS
// ==========================================

int ClampInt(int value, int minimum, int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;

    return value;
}

bool Intersects(RECT a, RECT b) {
    RECT result;

    return IntersectRect(&result, &a, &b) != FALSE;
}

void FillRectColor(
    HDC hdc,
    int left,
    int top,
    int right,
    int bottom,
    COLORREF color
) {
    HBRUSH brush = CreateSolidBrush(color);

    RECT rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;

    FillRect(hdc, &rect, brush);

    DeleteObject(brush);
}

void DrawTextCenter(
    HDC hdc,
    const wchar_t* message,
    int y,
    int fontSize,
    COLORREF color
) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    HFONT font = CreateFontW(
        fontSize,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Arial"
    );

    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    RECT rect;
    rect.left = 0;
    rect.top = y;
    rect.right = WIDTH;
    rect.bottom = y + fontSize + 15;

    DrawTextW(
        hdc,
        message,
        -1,
        &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// ==========================================
// 6. DRAW BACKGROUND
// ==========================================

void DrawBackground(HDC hdc) {

    FillRectColor(
        hdc,
        0, 0, WIDTH, HEIGHT,
        RGB(7, 10, 30)
    );

    // Stars

    for (int i = 0; i < 75; i++) {

        int x = (i * 137 + 43) % WIDTH;
        int y = (i * 97 + 19) % HEIGHT;

        COLORREF starColor = RGB(100, 120, 180);

        if (i % 3 == 0) {
            starColor = RGB(180, 210, 255);
        }

        FillRectColor(
            hdc,
            x, y,
            x + 2, y + 2,
            starColor
        );
    }
}

// ==========================================
// 7. DRAW PLAYER SPACESHIP
// ==========================================

void DrawPlayer(HDC hdc) {

    POINT ship[5];

    ship[0].x = (LONG)g_playerX;
    ship[0].y = (LONG)g_playerY - 25;

    ship[1].x = (LONG)g_playerX - 23;
    ship[1].y = (LONG)g_playerY + 20;

    ship[2].x = (LONG)g_playerX;
    ship[2].y = (LONG)g_playerY + 10;

    ship[3].x = (LONG)g_playerX + 23;
    ship[3].y = (LONG)g_playerY + 20;

    ship[4].x = (LONG)g_playerX;
    ship[4].y = (LONG)g_playerY - 25;

    HBRUSH brush = CreateSolidBrush(
        RGB(60, 210, 255)
    );

    HPEN pen = CreatePen(
        PS_SOLID,
        2,
        RGB(180, 245, 255)
    );

    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    Polygon(hdc, ship, 5);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);

    DeleteObject(brush);
    DeleteObject(pen);

    // Engine flame

    POINT flame[3];

    flame[0].x = (LONG)g_playerX - 7;
    flame[0].y = (LONG)g_playerY + 15;

    flame[1].x = (LONG)g_playerX + 7;
    flame[1].y = (LONG)g_playerY + 15;

    flame[2].x = (LONG)g_playerX;
    flame[2].y = (LONG)g_playerY + 29;

    HBRUSH flameBrush = CreateSolidBrush(
        RGB(255, 160, 40)
    );

    HBRUSH oldFlame = (HBRUSH)SelectObject(
        hdc, flameBrush
    );

    Polygon(hdc, flame, 3);

    SelectObject(hdc, oldFlame);
    DeleteObject(flameBrush);
}

// ==========================================
// 8. DRAW BULLETS
// ==========================================

void DrawBullets(HDC hdc) {

    for (size_t i = 0; i < g_bullets.size(); ++i) {

        const Bullet& b = g_bullets[i];

        HPEN pen = CreatePen(
            PS_SOLID,
            4,
            RGB(80, 245, 255)
        );

        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        MoveToEx(
            hdc,
            (int)b.x,
            (int)b.y,
            NULL
        );

        LineTo(
            hdc,
            (int)b.x,
            (int)b.y - 15
        );

        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

// ==========================================
// 9. DRAW ENEMIES
// ==========================================

void DrawEnemies(HDC hdc) {

    for (size_t i = 0; i < g_enemies.size(); ++i) {

        const Enemy& e = g_enemies[i];

        HBRUSH brush = CreateSolidBrush(
            RGB(245, 75, 95)
        );

        HPEN pen = CreatePen(
            PS_SOLID,
            2,
            RGB(255, 180, 185)
        );

        HBRUSH oldBrush = (HBRUSH)SelectObject(
            hdc, brush
        );

        HPEN oldPen = (HPEN)SelectObject(
            hdc, pen
        );

        Ellipse(
            hdc,
            (int)e.x - 19,
            (int)e.y - 16,
            (int)e.x + 19,
            (int)e.y + 16
        );

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);

        DeleteObject(brush);
        DeleteObject(pen);

        // Enemy eyes

        FillRectColor(
            hdc,
            (int)e.x - 10,
            (int)e.y - 3,
            (int)e.x - 4,
            (int)e.y + 3,
            RGB(255, 245, 170)
        );

        FillRectColor(
            hdc,
            (int)e.x + 4,
            (int)e.y - 3,
            (int)e.x + 10,
            (int)e.y + 3,
            RGB(255, 245, 170)
        );
    }
}

// ==========================================
// 10. DRAW GAME MENU AND SCREENS
// ==========================================

void RenderGame(HDC hdc) {

    DrawBackground(hdc);

    // MAIN MENU

    if (g_state == MENU) {

        DrawTextCenter(
            hdc,
            L"SPACE DEFENDER",
            110,
            48,
            RGB(70, 205, 255)
        );

        DrawTextCenter(
            hdc,
            L"Protect the galaxy. Survive the attack.",
            215,
            21,
            RGB(220, 230, 255)
        );

        DrawTextCenter(
            hdc,
            L"START GAME - Press ENTER",
            315,
            25,
            RGB(120, 255, 160)
        );

        DrawTextCenter(
            hdc,
            L"INSTRUCTIONS - Press I",
            365,
            21,
            RGB(245, 220, 110)
        );

        DrawTextCenter(
            hdc,
            L"Move: LEFT / RIGHT or A / D     Shoot: SPACE",
            500,
            18,
            RGB(190, 200, 225)
        );

        DrawTextCenter(
            hdc,
            L"Windows Programming Project - C++ Win32 API",
            565,
            16,
            RGB(120, 135, 165)
        );

        return;
    }

    // INSTRUCTIONS

    if (g_state == INSTRUCTIONS) {

        DrawTextCenter(
            hdc,
            L"HOW TO PLAY",
            145,
            38,
            RGB(70, 205, 255)
        );

        DrawTextCenter(
            hdc,
            L"LEFT / RIGHT or A / D - Move your spaceship",
            250,
            21,
            RGB(235, 240, 255)
        );

        DrawTextCenter(
            hdc,
            L"SPACE - Fire laser bullets",
            295,
            21,
            RGB(235, 240, 255)
        );

        DrawTextCenter(
            hdc,
            L"Destroy enemies before they reach your ship.",
            340,
            21,
            RGB(235, 240, 255)
        );

        DrawTextCenter(
            hdc,
            L"You have 3 lives. The game gets harder as your score grows.",
            385,
            19,
            RGB(235, 240, 255)
        );

        DrawTextCenter(
            hdc,
            L"Press ESC to return to the main menu",
            500,
            20,
            RGB(120, 255, 160)
        );

        return;
    }

    // GAME HUD

    wchar_t hud[160];

    wsprintfW(
        hud,
        L"SCORE: %d       HEALTH: %d       LEVEL: %d",
        g_score,
        g_health,
        g_level
    );

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 240, 255));

    HFONT hudFont = CreateFontW(
        21, 0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Arial"
    );

    HFONT oldFont = (HFONT)SelectObject(
        hdc, hudFont
    );

    TextOutW(
        hdc,
        22,
        18,
        hud,
        lstrlenW(hud)
    );

    SelectObject(hdc, oldFont);
    DeleteObject(hudFont);

    // Draw game objects

    DrawBullets(hdc);
    DrawEnemies(hdc);
    DrawPlayer(hdc);

    // GAME OVER SCREEN

    if (g_state == GAME_OVER) {

        FillRectColor(
            hdc,
            0, 0, WIDTH, HEIGHT,
            RGB(7, 10, 30)
        );

        DrawTextCenter(
            hdc,
            L"GAME OVER",
            205,
            48,
            RGB(255, 90, 110)
        );

        wchar_t scoreText[80];

        wsprintfW(
            scoreText,
            L"FINAL SCORE: %d",
            g_score
        );

        DrawTextCenter(
            hdc,
            scoreText,
            285,
            27,
            RGB(240, 240, 255)
        );

        DrawTextCenter(
            hdc,
            L"Press ENTER to play again",
            370,
            22,
            RGB(120, 255, 160)
        );

        DrawTextCenter(
            hdc,
            L"Press ESC to return to menu",
            415,
            19,
            RGB(190, 200, 225)
        );
    }
}

// ==========================================
// 11. START OR RESTART GAME
// ==========================================

void StartGame() {

    g_score = 0;
    g_health = MAX_HEALTH;
    g_level = 1;

    g_playerX = WIDTH / 2.0f;
    g_playerY = HEIGHT - 65.0f;

    g_bullets.clear();
    g_enemies.clear();

    DWORD now = GetTickCount();

    g_lastShot = now;
    g_lastSpawn = now;
    g_lastLevel = now;

    for (int i = 0; i < 256; ++i) {
        g_keys[i] = false;
    }

    g_state = PLAYING;

    InvalidateRect(g_hwnd, NULL, FALSE);
}

// ==========================================
// 12. UPDATE GAME
// ==========================================

void UpdateGame() {

    if (g_state != PLAYING) {
        return;
    }

    DWORD now = GetTickCount();

    // Player movement

    if (g_keys[VK_LEFT] || g_keys['A']) {
        g_playerX -= 7.0f;
    }

    if (g_keys[VK_RIGHT] || g_keys['D']) {
        g_playerX += 7.0f;
    }

    g_playerX = (float)ClampInt(
        (int)g_playerX,
        28,
        WIDTH - 28
    );

    // Shooting

    if (g_keys[VK_SPACE] &&
        now - g_lastShot > 260) {

        g_bullets.push_back(
            Bullet(g_playerX, g_playerY - 28)
        );

        g_lastShot = now;
    }

    // Spawn enemies

    DWORD spawnDelay =
        (DWORD)(900 - (g_level - 1) * 70);

    if (now - g_lastSpawn > spawnDelay) {

        float x = (float)(
            35 + rand() % (WIDTH - 70)
        );

        float speed = 2.0f + g_level * 0.45f;

        g_enemies.push_back(
            Enemy(x, 65.0f, speed)
        );

        g_lastSpawn = now;
    }

    // Move bullets

    for (size_t i = 0; i < g_bullets.size(); ++i) {
        g_bullets[i].y -= 10.0f;
    }

    // Remove bullets outside the screen

    for (size_t i = 0; i < g_bullets.size();) {

        if (g_bullets[i].y < -20) {

            g_bullets.erase(
                g_bullets.begin() + i
            );

        } else {
            ++i;
        }
    }

    // Move enemies

    for (size_t i = 0; i < g_enemies.size(); ++i) {
        g_enemies[i].y += g_enemies[i].speed;
    }

    // Bullet-enemy collisions

    for (size_t i = 0; i < g_enemies.size();) {

        RECT enemyRect = {
            (LONG)g_enemies[i].x - 18,
            (LONG)g_enemies[i].y - 15,
            (LONG)g_enemies[i].x + 18,
            (LONG)g_enemies[i].y + 15
        };

        bool destroyed = false;

        for (size_t j = 0; j < g_bullets.size(); ++j) {

            RECT bulletRect = {
                (LONG)g_bullets[j].x - 3,
                (LONG)g_bullets[j].y - 15,
                (LONG)g_bullets[j].x + 3,
                (LONG)g_bullets[j].y
            };

            if (Intersects(enemyRect, bulletRect)) {

                g_bullets.erase(
                    g_bullets.begin() + j
                );

                g_enemies.erase(
                    g_enemies.begin() + i
                );

                g_score += 10;

                destroyed = true;

                break;
            }
        }

        if (!destroyed) {
            ++i;
        }
    }

    // Player-enemy collisions

    RECT playerRect = {
        (LONG)g_playerX - 20,
        (LONG)g_playerY - 20,
        (LONG)g_playerX + 20,
        (LONG)g_playerY + 23
    };

    for (size_t i = 0; i < g_enemies.size();) {

        RECT enemyRect = {
            (LONG)g_enemies[i].x - 18,
            (LONG)g_enemies[i].y - 15,
            (LONG)g_enemies[i].x + 18,
            (LONG)g_enemies[i].y + 15
        };

        if (Intersects(playerRect, enemyRect) ||
            g_enemies[i].y > HEIGHT + 20) {

            g_enemies.erase(
                g_enemies.begin() + i
            );

            --g_health;

        } else {
            ++i;
        }
    }

    // Increase level every 12 seconds

    if (now - g_lastLevel > 12000) {

        if (g_level < 8) {
            ++g_level;
        }

        g_lastLevel = now;
    }

    // Game over

    if (g_health <= 0) {
        g_health = 0;
        g_state = GAME_OVER;
    }

    InvalidateRect(g_hwnd, NULL, FALSE);
}

// ==========================================
// 13. WINDOW PROCEDURE
// ==========================================

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {

    switch (msg) {

    case WM_CREATE:

        g_hwnd = hwnd;

        srand((unsigned int)time(NULL));

        SetTimer(
            hwnd,
            GAME_TIMER,
            16,
            NULL
        );

        return 0;

    case WM_TIMER:

        if (wParam == GAME_TIMER) {
            UpdateGame();
        }

        return 0;

    case WM_KEYDOWN:

        if (wParam < 256) {
            g_keys[wParam] = true;
        }

        // Main menu

        if (g_state == MENU) {

            if (wParam == VK_RETURN) {
                StartGame();
            }

            if (wParam == 'I') {
                g_state = INSTRUCTIONS;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }

        // Instructions

        else if (g_state == INSTRUCTIONS) {

            if (wParam == VK_ESCAPE) {
                g_state = MENU;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }

        // Playing

        else if (g_state == PLAYING) {

            if (wParam == VK_ESCAPE) {

                g_state = MENU;

                for (int i = 0; i < 256; ++i) {
                    g_keys[i] = false;
                }

                InvalidateRect(hwnd, NULL, FALSE);
            }
        }

        // Game over

        else if (g_state == GAME_OVER) {

            if (wParam == VK_RETURN) {
                StartGame();
            }

            if (wParam == VK_ESCAPE) {
                g_state = MENU;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }

        return 0;

    case WM_KEYUP:

        if (wParam < 256) {
            g_keys[wParam] = false;
        }

        return 0;

    case WM_ERASEBKGND:

        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC hdc = BeginPaint(hwnd, &ps);

        RenderGame(hdc);

        EndPaint(hwnd, &ps);

        return 0;
    }

    case WM_DESTROY:

        KillTimer(hwnd, GAME_TIMER);

        PostQuitMessage(0);

        return 0;
    }

    return DefWindowProc(
        hwnd,
        msg,
        wParam,
        lParam
    );
}
