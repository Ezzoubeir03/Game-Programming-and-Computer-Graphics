
#include <windows.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

struct Bullet {
    float x, y;
    Bullet(float px, float py) : x(px), y(py) {}
};

struct Enemy {
    float x, y, speed;
    Enemy(float px, float py, float pspeed) : x(px), y(py), speed(pspeed) {}
};

enum GameState { MENU, PLAYING, INSTRUCTIONS, GAME_OVER };

static const int WIDTH = 900;
static const int HEIGHT = 650;
static const UINT_PTR GAME_TIMER = 1;

HWND g_hwnd = nullptr;
GameState g_state = MENU;
std::vector<Bullet> g_bullets;
std::vector<Enemy> g_enemies;
bool g_keys[256] = {};
float g_playerX = WIDTH / 2.0f;
const float g_playerY = HEIGHT - 85.0f;
int g_score = 0;
int g_health = 3;
int g_level = 1;
DWORD g_lastShot = 0;
DWORD g_lastSpawn = 0;
DWORD g_lastLevel = 0;

int ClampInt(int value, int low, int high) {
    return (std::max)(low, (std::min)(value, high));
}

bool Intersects(RECT a, RECT b) {
    return a.left < b.right && a.right > b.left &&
           a.top < b.bottom && a.bottom > b.top;
}

void StartGame() {
    g_bullets.clear();
    g_enemies.clear();
    ZeroMemory(g_keys, sizeof(g_keys));
    g_playerX = WIDTH / 2.0f;
    g_score = 0;
    g_health = 3;
    g_level = 1;
    DWORD now = GetTickCount();
    g_lastShot = now;
    g_lastSpawn = now;
    g_lastLevel = now;
    g_state = PLAYING;
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

void DrawTextCenter(HDC hdc, const wchar_t* text, int y, int fontSize, COLORREF color) {
    HFONT font = CreateFontW(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT r = { 0, y, WIDTH, y + fontSize + 20 };
    DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void FillRectColor(HDC hdc, int left, int top, int right, int bottom, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT r = { left, top, right, bottom };
    FillRect(hdc, &r, brush);
    DeleteObject(brush);
}

void DrawPlayer(HDC hdc) {
    POINT ship[] = {
        { (LONG)g_playerX, (LONG)(g_playerY - 27) },
        { (LONG)(g_playerX - 23), (LONG)(g_playerY + 22) },
        { (LONG)g_playerX, (LONG)(g_playerY + 12) },
        { (LONG)(g_playerX + 23), (LONG)(g_playerY + 22) }
    };
    HBRUSH brush = CreateSolidBrush(RGB(40, 190, 255));
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(190, 240, 255));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
