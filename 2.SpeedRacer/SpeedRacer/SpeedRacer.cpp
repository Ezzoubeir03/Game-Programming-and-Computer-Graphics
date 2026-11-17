#include <windows.h>
#include <tchar.h>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "winmm.lib")

const int WINDOW_W = 800;
const int WINDOW_H = 600;
const int ROAD_LEFT = 220;
const int ROAD_RIGHT = 580;
const int PLAYER_W = 48;
const int PLAYER_H = 82;
const int ENEMY_COUNT = 5;
const int WIN_SCORE = 500;

enum GameState
{
    MENU,
    PLAYING,
    GAME_OVER,
    VICTORY
};

struct Car
{
    int x;
    int y;
    int speed;
    bool active;
};

HWND g_hwnd = NULL;
GameState g_state = MENU;
Car g_player;
Car g_enemies[ENEMY_COUNT];
bool g_leftPressed = false;
bool g_rightPressed = false;
int g_score = 0;
int g_level = 1;
int g_roadOffset = 0;
int g_spawnCounter = 0;

void PlayEventSound(DWORD sound)
{
    MessageBeep(sound);
}

void ResetGame()
{
    g_player.x = 376;
    g_player.y = 475;
    g_player.speed = 7;
    g_player.active = true;

    for (int i = 0; i < ENEMY_COUNT; ++i)
    {
        g_enemies[i].x = ROAD_LEFT + 35 + (rand() % 250);
        g_enemies[i].y = -150 - (i * 150);
        g_enemies[i].speed = 4;
        g_enemies[i].active = true;
    }

    g_score = 0;
    g_level = 1;
    g_roadOffset = 0;
    g_spawnCounter = 0;
    g_leftPressed = false;
    g_rightPressed = false;
}

void DrawTextCentered(HDC hdc, LPCTSTR text, int y, int fontSize, COLORREF color)
{
    HFONT font = CreateFont(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, TEXT("Arial"));

    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    RECT r;
    r.left = 0;
    r.top = y;
    r.right = WINDOW_W;
    r.bottom = y + fontSize + 20;

    DrawText(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void FillRectColor(HDC hdc, int left, int top, int right, int bottom, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    RECT r;
    r.left = left;
    r.top = top;
    r.right = right;
    r.bottom = bottom;
    FillRect(hdc, &r, brush);
    DeleteObject(brush);
}

void DrawCar(HDC hdc, int x, int y, COLORREF bodyColor, bool player)
{
    // Wheels
    FillRectColor(hdc, x - 5, y + 12, x + 5, y + 31, RGB(20, 20, 20));
    FillRectColor(hdc, x + PLAYER_W - 5, y + 12, x + PLAYER_W + 5, y + 31, RGB(20, 20, 20));
    FillRectColor(hdc, x - 5, y + 53, x + 5, y + 72, RGB(20, 20, 20));
    FillRectColor(hdc, x + PLAYER_W - 5, y + 53, x + PLAYER_W + 5, y + 72, RGB(20, 20, 20));

    // Main body
    FillRectColor(hdc, x, y, x + PLAYER_W, y + PLAYER_H, bodyColor);

    // Windows
    FillRectColor(hdc, x + 8, y + 13, x + PLAYER_W - 8, y + 34, RGB(150, 210, 230));
    FillRectColor(hdc, x + 8, y + 43, x + PLAYER_W - 8, y + 62, RGB(80, 110, 130));

    // Lights
    FillRectColor(hdc, x + 5, y + 3, x + 15, y + 8, RGB(255, 245, 150));
    FillRectColor(hdc, x + PLAYER_W - 15, y + 3, x + PLAYER_W - 5, y + 8, RGB(255, 245, 150));

    if (player)
    {
        FillRectColor(hdc, x + 17, y + 68, x + 31, y + 76, RGB(255, 255, 255));
    }
}

void DrawRoad(HDC hdc)
{
    // Grass
    FillRectColor(hdc, 0, 0, WINDOW_W, WINDOW_H, RGB(35, 135, 65));

    // Road
    FillRectColor(hdc, ROAD_LEFT, 0, ROAD_RIGHT, WINDOW_H, RGB(55, 55, 55));

    // Road edges
    FillRectColor(hdc, ROAD_LEFT, 0, ROAD_LEFT + 8, WINDOW_H, RGB(230, 230, 230));
    FillRectColor(hdc, ROAD_RIGHT - 8, 0, ROAD_RIGHT, WINDOW_H, RGB(230, 230, 230));

    // Dashed lane markings
    for (int y = -60 + g_roadOffset; y < WINDOW_H; y += 100)
    {
        FillRectColor(hdc, 337, y, 343, y + 48, RGB(245, 245, 245));
        FillRectColor(hdc, 457, y, 463, y + 48, RGB(245, 245, 245));
    }
}

bool CarsCollide(const Car& a, const Car& b)
{
    RECT r1 = { a.x + 5, a.y + 5, a.x + PLAYER_W - 5, a.y + PLAYER_H - 5 };
    RECT r2 = { b.x + 5, b.y + 5, b.x + PLAYER_W - 5, b.y + PLAYER_H - 5 };
    RECT intersection;

    return IntersectRect(&intersection, &r1, &r2) != 0;
}

void UpdateLevel()
{
    if (g_score < 150)
        g_level = 1;
    else if (g_score < 350)
        g_level = 2;
    else
        g_level = 3;
}

void SpawnEnemy(Car& enemy)
{
    enemy.x = ROAD_LEFT + 35 + (rand() % 250);
    enemy.y = -PLAYER_H;
    enemy.speed = 4 + g_level * 2;
    enemy.active = true;
}

void UpdateGame()
{
    if (g_state != PLAYING)
        return;

    if (g_leftPressed && g_player.x > ROAD_LEFT + 15)
        g_player.x -= g_player.speed;

    if (g_rightPressed && g_player.x + PLAYER_W < ROAD_RIGHT - 15)
        g_player.x += g_player.speed;

    g_roadOffset += 8 + g_level * 2;
    if (g_roadOffset >= 100)
        g_roadOffset = 0;

    ++g_spawnCounter;

    for (int i = 0; i < ENEMY_COUNT; ++i)
    {
        if (g_enemies[i].active)
        {
            g_enemies[i].y += g_enemies[i].speed;

            if (CarsCollide(g_player, g_enemies[i]))
            {
                g_state = GAME_OVER;
                PlayEventSound(MB_ICONHAND);
                return;
            }

            if (g_enemies[i].y > WINDOW_H)
            {
                SpawnEnemy(g_enemies[i]);
                g_score += 10;
                UpdateLevel();

                if (g_score >= WIN_SCORE)
                {
                    g_state = VICTORY;
                    PlayEventSound(MB_ICONASTERISK);
                    return;
                }
            }
        }
    }

    // Increase enemy speed as the level increases.
    for (int j = 0; j < ENEMY_COUNT; ++j)
        g_enemies[j].speed = 3 + g_level * 2;
}

void DrawHUD(HDC hdc)
{
    TCHAR scoreText[80];
    TCHAR levelText[80];

    wsprintf(scoreText, TEXT("SCORE: %d"), g_score);
    wsprintf(levelText, TEXT("LEVEL: %d"), g_level);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    HFONT font = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, TEXT("Arial"));

    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    TextOut(hdc, 20, 20, scoreText, lstrlen(scoreText));
    TextOut(hdc, 20, 52, levelText, lstrlen(levelText));

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void DrawMenu(HDC hdc)
{
    FillRectColor(hdc, 0, 0, WINDOW_W, WINDOW_H, RGB(20, 30, 45));

    DrawTextCentered(hdc, TEXT("SPEED RACER"), 115, 52, RGB(255, 210, 40));
    DrawTextCentered(hdc, TEXT("A CLASSIC 2D CAR RACING GAME"), 190, 22, RGB(230, 230, 230));
    DrawTextCentered(hdc, TEXT("PRESS ENTER TO START"), 310, 28, RGB(80, 230, 130));
    DrawTextCentered(hdc, TEXT("MOVE: LEFT / RIGHT ARROW KEYS OR A / D"), 380, 18, RGB(255, 255, 255));
    DrawTextCentered(hdc, TEXT("AVOID OTHER CARS AND REACH 500 POINTS"), 415, 18, RGB(255, 255, 255));
    DrawTextCentered(hdc, TEXT("PRESS ESC TO QUIT"), 500, 16, RGB(190, 190, 190));
}

void DrawEndScreen(HDC hdc, bool victory)
{
    FillRectColor(hdc, 0, 0, WINDOW_W, WINDOW_H, RGB(20, 30, 45));

    if (victory)
    {
        DrawTextCentered(hdc, TEXT("VICTORY!"), 150, 52, RGB(80, 240, 120));
        DrawTextCentered(hdc, TEXT("YOU ARE THE RACING CHAMPION!"), 235, 25, RGB(255, 255, 255));
    }
    else
    {
        DrawTextCentered(hdc, TEXT("GAME OVER"), 150, 52, RGB(255, 80, 80));
        DrawTextCentered(hdc, TEXT("YOU CRASHED!"), 235, 25, RGB(255, 255, 255));
    }

    TCHAR scoreText[80];
    wsprintf(scoreText, TEXT("FINAL SCORE: %d"), g_score);
    DrawTextCentered(hdc, scoreText, 300, 25, RGB(255, 220, 70));
    DrawTextCentered(hdc, TEXT("PRESS R TO PLAY AGAIN"), 390, 22, RGB(100, 220, 255));
    DrawTextCentered(hdc, TEXT("PRESS M FOR MAIN MENU"), 430, 20, RGB(230, 230, 230));
}

void DrawPlaying(HDC hdc)
{
    DrawRoad(hdc);

    for (int i = 0; i < ENEMY_COUNT; ++i)
    {
        if (g_enemies[i].active)
            DrawCar(hdc, g_enemies[i].x, g_enemies[i].y, RGB(190, 45, 45), false);
    }

    DrawCar(hdc, g_player.x, g_player.y, RGB(35, 120, 245), true);
    DrawHUD(hdc);
}

void PaintScene(HDC hdc)
{
    if (g_state == MENU)
        DrawMenu(hdc);
    else if (g_state == PLAYING)
        DrawPlaying(hdc);
    else if (g_state == GAME_OVER)
        DrawEndScreen(hdc, false);
    else if (g_state == VICTORY)
        DrawEndScreen(hdc, true);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 30, NULL);
        return 0;

    case WM_TIMER:
        UpdateGame();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
            return 0;
        }

        if (g_state == MENU && wParam == VK_RETURN)
        {
            ResetGame();
            g_state = PLAYING;
            PlayEventSound(MB_OK);
        }
        else if (g_state == PLAYING)
        {
            if (wParam == VK_LEFT || wParam == 'A')
                g_leftPressed = true;
            if (wParam == VK_RIGHT || wParam == 'D')
                g_rightPressed = true;
        }
        else if ((g_state == GAME_OVER || g_state == VICTORY) && wParam == 'R')
        {
            ResetGame();
            g_state = PLAYING;
            PlayEventSound(MB_OK);
        }
        else if ((g_state == GAME_OVER || g_state == VICTORY) && wParam == 'M')
        {
            g_state = MENU;
        }

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_KEYUP:
        if (wParam == VK_LEFT || wParam == 'A')
            g_leftPressed = false;
        if (wParam == VK_RIGHT || wParam == 'D')
            g_rightPressed = false;
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client;
            GetClientRect(hwnd, &client);

            HDC memoryDC = CreateCompatibleDC(hdc);
            HBITMAP memoryBitmap = CreateCompatibleBitmap(hdc,
                client.right - client.left, client.bottom - client.top);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memoryDC, memoryBitmap);

            PaintScene(memoryDC);

            BitBlt(hdc, 0, 0, client.right, client.bottom,
                memoryDC, 0, 0, SRCCOPY);

            SelectObject(memoryDC, oldBitmap);
            DeleteObject(memoryBitmap);
            DeleteDC(memoryDC);

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    srand((unsigned int)time(NULL));

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = TEXT("SpeedRacerWindowClass");

    if (!RegisterClass(&wc))
        return 0;

    RECT r = { 0, 0, WINDOW_W, WINDOW_H };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindow(
        TEXT("SpeedRacerWindowClass"),
        TEXT("Speed Racer - C++ Win32 GDI"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
        return 0;

    g_hwnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
