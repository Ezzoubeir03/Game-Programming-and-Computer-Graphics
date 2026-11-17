// ============================================================================
//  StarCatcher - catch the falling stars!
//
//  How to play
//    * Stars fall from the sky. Walk left / right and catch them.
//    * Yellow star  = +1 point          Golden star = +5 points
//    * Purple star  = BAD! Dodge it.    (catching it costs a life)
//    * Missing a yellow or golden star also costs a life.
//    * Catch stars in a row to build a COMBO and multiply your score (up to x4).
//    * Every 20 stars in a row gives you a life back.
//    * The game gets faster as your level goes up.
//
//  Controls
//    Left / Right arrows or A / D : move        Shift : run
//    Left-click : walk to that spot             P or right-click : pause
//    Enter / Space : start / play again         M : music    N : sound effects
//    Esc : quit
//
//  Files (next to this .cpp, or next to the .exe)
//    bg.bmp, go1..go4.bmp      scenery and character sheets (8 frames of 60x108,
//                              colour on top / mask below)
//    bgm.wav                   background music
//    catch.wav gold.wav bad.wav miss.wav levelup.wav gameover.wav   sound effects
//    starcatcher.ini           created by the game to remember your high score
// ============================================================================

#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM
#include <math.h>
#include <stdlib.h>

#pragma comment(lib, "winmm.lib")      // timeBeginPeriod, PlaySound, MCI
#pragma comment(lib, "msimg32.lib")    // AlphaBlend (see-through dark overlay)

// ----------------------------------------------------------------------------
//  Settings
// ----------------------------------------------------------------------------
#define WINDOW_WIDTH   1000
#define WINDOW_HEIGHT  800
#define WINDOW_TITLE   L"Star Catcher"

const float PI = 3.14159265f;

// Character sprite sheets: 8 frames of 60x108 (colour on top, mask below)
const int   FRAME_W      = 60;
const int   FRAME_H      = 108;
const int   FRAME_COUNT  = 8;
const float SPRITE_SCALE = 1.5f;
enum Direction { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3 };

const float WALK_SPEED      = 280.0f;   // pixels per second
const float RUN_SPEED       = 480.0f;
const float WALK_FRAME_TIME = 0.08f;
const float RUN_FRAME_TIME  = 0.05f;
const int   GROUND_MARGIN   = 25;       // feet are this far above the window bottom

// The area of the character that catches stars (relative to the feet)
const float CATCH_HALF_W = 34.0f;
const float CATCH_TOP    = 165.0f;      // top of the head
const float CATCH_BOTTOM = 60.0f;       // waist

// Rules
const int MAX_LIVES        = 5;
const int STARS_PER_LEVEL  = 8;
const int COMBO_FOR_LIFE   = 20;

const int MAX_STARS     = 48;
const int MAX_PARTICLES = 400;
const int MAX_TEXTS     = 24;

// ----------------------------------------------------------------------------
//  Types
// ----------------------------------------------------------------------------
enum GameState { STATE_TITLE, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER };
enum StarType  { STAR_NORMAL, STAR_GOLD, STAR_BAD };

enum Sfx { SFX_CATCH, SFX_GOLD, SFX_BAD, SFX_MISS, SFX_LEVELUP, SFX_GAMEOVER, SFX_COUNT };
static const wchar_t* SFX_FILES[SFX_COUNT] =
{
    L"catch.wav", L"gold.wav", L"bad.wav", L"miss.wav", L"levelup.wav", L"gameover.wav"
};

struct Player
{
    float x, y;            // feet position (bottom centre of the sprite)
    Direction dir;
    int   frame;
    float animTimer;
    bool  moving, running;
    bool  hasTarget;
    float targetX;
};

struct Star
{
    bool     alive;
    StarType type;
    float    baseX, x, y;      // x = baseX + a gentle side-to-side sway
    float    vy;               // fall speed (pixels per second)
    float    r;                // radius
    float    angle, spin;
    float    age, swayAmp, swayFreq, swayPhase;
};

struct Particle
{
    bool     alive;
    float    x, y, vx, vy, life, maxLife;
    COLORREF color;
    int      size;
};

struct FloatText
{
    bool     alive;
    float    x, y, life;
    wchar_t  text[24];
    COLORREF color;
};

// ----------------------------------------------------------------------------
//  Globals
// ----------------------------------------------------------------------------
HINSTANCE g_hInst = NULL;
HWND      g_hWnd  = NULL;

HBITMAP g_bgBitmap = NULL;
HBITMAP g_goBitmaps[4] = { NULL, NULL, NULL, NULL };

// Double buffering
HDC     g_backDC = NULL;   HBITMAP g_backBitmap = NULL;
HDC     g_bgDC   = NULL;   HBITMAP g_bgScaled   = NULL;
int     g_clientW = 0, g_clientH = 0;

// Fonts, pens and brushes (created once)
HFONT   g_fontSmall = NULL, g_fontMed = NULL, g_fontBig = NULL;
HPEN    g_penNormal, g_penGold, g_penBad, g_penHeart, g_penGrey;
HBRUSH  g_brNormal, g_brGold, g_brBad, g_brHeart;

// Game
GameState g_state = STATE_TITLE;
Player    g_player;
Star      g_stars[MAX_STARS];
Particle  g_particles[MAX_PARTICLES];
FloatText g_texts[MAX_TEXTS];

int   g_score = 0, g_highScore = 0, g_lives = MAX_LIVES;
int   g_combo = 0, g_bestCombo = 0, g_level = 1, g_goodCaught = 0;
bool  g_newHigh = false;
float g_spawnTimer = 0.0f;
float g_time = 0.0f;           // seconds since the program started
int   g_fps = 0;

// Sound
bool    g_musicOpen = false, g_musicOn = true, g_sfxOn = true;
wchar_t g_sfxPath[SFX_COUNT][MAX_PATH];

// ----------------------------------------------------------------------------
//  Declarations
// ----------------------------------------------------------------------------
ATOM    MyWindowsClass(HINSTANCE hInstance);
BOOL    InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void LoadImages();        void ReleaseImages();
void RebuildBuffers(HWND hWnd);  void ReleaseBuffers();
void CreateGraphics();    void ReleaseGraphics();
void LoadSounds();
void StartMusic();        void StopMusic();
void SetMusicOn(bool on); void KeepMusicLooping();
void LoadHighScore();     void SaveHighScore();

void StartGame();
void TogglePause();
void Update(float dt);
void RenderFrame();
void Present(HDC hdc);

// ----------------------------------------------------------------------------
//  Small helpers
// ----------------------------------------------------------------------------
static float RandF(float a, float b)
{
    return a + (b - a) * ((float)rand() / (float)RAND_MAX);
}

static bool KeyDown(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static float GroundY()
{
    return (float)(g_clientH - GROUND_MARGIN);
}

// Looks for a file in the current directory, then next to the .exe.
static bool ResolveFile(const wchar_t* name, wchar_t* out, int size)
{
    wchar_t full[MAX_PATH];

    if (GetFullPathName(name, MAX_PATH, full, NULL) > 0 &&
        GetFileAttributes(full) != INVALID_FILE_ATTRIBUTES)
    {
        wcscpy_s(out, size, full);
        return true;
    }

    if (GetModuleFileName(NULL, full, MAX_PATH) > 0)
    {
        wchar_t* slash = wcsrchr(full, L'\\');
        if (slash != NULL && wcslen(full) + wcslen(name) + 1 < MAX_PATH)
        {
            *(slash + 1) = 0;
            wcscat_s(full, MAX_PATH, name);
            if (GetFileAttributes(full) != INVALID_FILE_ATTRIBUTES)
            {
                wcscpy_s(out, size, full);
                return true;
            }
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
//  Entry point + game loop
// ----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!MyWindowsClass(hInstance))
    {
        MessageBox(NULL, L"Failed to register window class.", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!InitInstance(hInstance, nCmdShow))
    {
        return 1;
    }

    timeBeginPeriod(1);

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
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (quit) break;

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;

        if (dt < TARGET_DT)
        {
            Sleep(1);
            continue;
        }
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        Update(dt);
        RenderFrame();

        HDC hdc = GetDC(g_hWnd);
        Present(hdc);
        ReleaseDC(g_hWnd, hdc);

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
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"starcatcher";
    return RegisterClassEx(&wc);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_hInst = hInstance;

    g_hWnd = CreateWindow(L"starcatcher", WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          WINDOW_WIDTH, WINDOW_HEIGHT,
                          NULL, NULL, hInstance, NULL);
    if (g_hWnd == NULL)
    {
        return FALSE;
    }

    srand(GetTickCount());

    CreateGraphics();
    LoadImages();
    RebuildBuffers(g_hWnd);
    LoadSounds();
    LoadHighScore();

    g_player.x = g_clientW / 2.0f;
    g_player.dir = DIR_DOWN;
    g_player.frame = 0;
    g_player.y = GroundY();

    StartMusic();

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return TRUE;
}

// ----------------------------------------------------------------------------
//  Graphics objects, images, back buffer
// ----------------------------------------------------------------------------
void CreateGraphics()
{
    g_fontSmall = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    g_fontMed   = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    g_fontBig   = CreateFont(78, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    g_penNormal = CreatePen(PS_SOLID, 2, RGB(205, 140, 0));
    g_brNormal  = CreateSolidBrush(RGB(255, 215, 40));
    g_penGold   = CreatePen(PS_SOLID, 3, RGB(255, 120, 0));
    g_brGold    = CreateSolidBrush(RGB(255, 245, 130));
    g_penBad    = CreatePen(PS_SOLID, 3, RGB(235, 40, 70));
    g_brBad     = CreateSolidBrush(RGB(75, 30, 115));
    g_penHeart  = CreatePen(PS_SOLID, 2, RGB(120, 10, 30));
    g_brHeart   = CreateSolidBrush(RGB(235, 40, 70));
    g_penGrey   = CreatePen(PS_SOLID, 2, RGB(150, 150, 150));
}

void ReleaseGraphics()
{
    HGDIOBJ objs[] = { g_fontSmall, g_fontMed, g_fontBig, g_penNormal, g_brNormal,
                       g_penGold, g_brGold, g_penBad, g_brBad, g_penHeart,
                       g_brHeart, g_penGrey };
    for (int i = 0; i < (int)(sizeof(objs) / sizeof(objs[0])); i++)
    {
        if (objs[i] != NULL) DeleteObject(objs[i]);
    }
}

static HBITMAP LoadBmp(const wchar_t* name)
{
    wchar_t path[MAX_PATH];
    if (!ResolveFile(name, path, MAX_PATH))
    {
        return NULL;
    }
    return (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0,
                              LR_LOADFROMFILE | LR_CREATEDIBSECTION);
}

void LoadImages()
{
    static const wchar_t* goNames[4] = { L"go1.bmp", L"go2.bmp", L"go3.bmp", L"go4.bmp" };
    wchar_t missing[256] = L"";

    g_bgBitmap = LoadBmp(L"bg.bmp");
    if (g_bgBitmap == NULL) wcscat_s(missing, 256, L"  bg.bmp\n");

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
        wcscat_s(text, 512, L"\nPut them next to StarCatcher.cpp (or next to the .exe).");
        MessageBox(g_hWnd, text, L"Image Loading Error", MB_ICONWARNING);
    }
}

void ReleaseImages()
{
    if (g_bgBitmap) { DeleteObject(g_bgBitmap); g_bgBitmap = NULL; }
    for (int i = 0; i < 4; i++)
    {
        if (g_goBitmaps[i]) { DeleteObject(g_goBitmaps[i]); g_goBitmaps[i] = NULL; }
    }
}

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

    g_backDC = CreateCompatibleDC(screen);
    g_backBitmap = CreateCompatibleBitmap(screen, g_clientW, g_clientH);
    SelectObject(g_backDC, g_backBitmap);
    SetBkMode(g_backDC, TRANSPARENT);

    // Background stretched to the window once (not every frame)
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
        HBRUSH sky = CreateSolidBrush(RGB(20, 24, 60));
        RECT all = { 0, 0, g_clientW, g_clientH };
        FillRect(g_bgDC, &all, sky);
        DeleteObject(sky);
    }

    ReleaseDC(hWnd, screen);
}

// ----------------------------------------------------------------------------
//  Sound
// ----------------------------------------------------------------------------
void LoadSounds()
{
    for (int i = 0; i < SFX_COUNT; i++)
    {
        g_sfxPath[i][0] = 0;
        ResolveFile(SFX_FILES[i], g_sfxPath[i], MAX_PATH);
    }
}

static void PlaySfx(Sfx s)
{
    if (g_sfxOn && g_sfxPath[s][0] != 0)
    {
        PlaySound(g_sfxPath[s], NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

void StartMusic()
{
    wchar_t path[MAX_PATH];
    if (!ResolveFile(L"bgm.wav", path, MAX_PATH))
    {
        return;
    }

    wchar_t command[MAX_PATH + 64];
    wsprintf(command, L"open \"%s\" type waveaudio alias bgm", path);
    if (mciSendString(command, NULL, 0, NULL) != 0)
    {
        return;
    }

    g_musicOpen = true;
    if (g_musicOn)
    {
        mciSendString(L"play bgm notify", NULL, 0, g_hWnd);
    }
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

void SetMusicOn(bool on)
{
    g_musicOn = on;
    if (!g_musicOpen) return;

    if (on)
        mciSendString(L"play bgm notify", NULL, 0, g_hWnd);
    else
        mciSendString(L"pause bgm", NULL, 0, NULL);
}

// Safety net: if the song stopped for any reason, start it again.
void KeepMusicLooping()
{
    if (!g_musicOpen || !g_musicOn) return;

    wchar_t mode[64] = L"";
    mciSendString(L"status bgm mode", mode, 64, NULL);
    if (wcscmp(mode, L"stopped") == 0)
    {
        mciSendString(L"seek bgm to start", NULL, 0, NULL);
        mciSendString(L"play bgm notify", NULL, 0, g_hWnd);
    }
}

// ----------------------------------------------------------------------------
//  High score (saved in starcatcher.ini next to the .exe)
// ----------------------------------------------------------------------------
static void GetIniPath(wchar_t* out, int size)
{
    out[0] = 0;
    GetModuleFileName(NULL, out, size);
    wchar_t* slash = wcsrchr(out, L'\\');
    if (slash != NULL) *(slash + 1) = 0;
    wcscat_s(out, size, L"starcatcher.ini");
}

void LoadHighScore()
{
    wchar_t path[MAX_PATH];
    GetIniPath(path, MAX_PATH);
    g_highScore = (int)GetPrivateProfileInt(L"StarCatcher", L"HighScore", 0, path);
}

void SaveHighScore()
{
    wchar_t path[MAX_PATH], value[32];
    GetIniPath(path, MAX_PATH);
    wsprintf(value, L"%d", g_highScore);
    WritePrivateProfileString(L"StarCatcher", L"HighScore", value, path);
}

// ----------------------------------------------------------------------------
//  Effects: floating text and particles
// ----------------------------------------------------------------------------
static void AddText(float x, float y, const wchar_t* text, COLORREF color)
{
    for (int i = 0; i < MAX_TEXTS; i++)
    {
        FloatText& t = g_texts[i];
        if (!t.alive)
        {
            t.alive = true;
            t.x = x; t.y = y; t.life = 1.1f; t.color = color;
            wcscpy_s(t.text, 24, text);
            return;
        }
    }
}

static void Burst(float x, float y, int count, COLORREF c1, COLORREF c2,
                  float speed, int size)
{
    for (int n = 0; n < count; n++)
    {
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            Particle& p = g_particles[i];
            if (!p.alive)
            {
                float a = RandF(0.0f, 2.0f * PI);
                float s = RandF(speed * 0.3f, speed);
                p.alive = true;
                p.x = x; p.y = y;
                p.vx = cosf(a) * s;
                p.vy = sinf(a) * s - speed * 0.3f;
                p.maxLife = p.life = RandF(0.4f, 0.9f);
                p.color = (rand() & 1) ? c1 : c2;
                p.size = size;
                break;
            }
        }
    }
}

static void UpdateEffects(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle& p = g_particles[i];
        if (!p.alive) continue;
        p.life -= dt;
        if (p.life <= 0.0f) { p.alive = false; continue; }
        p.vy += 520.0f * dt;                // gravity
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
    for (int i = 0; i < MAX_TEXTS; i++)
    {
        FloatText& t = g_texts[i];
        if (!t.alive) continue;
        t.life -= dt;
        if (t.life <= 0.0f) { t.alive = false; continue; }
        t.y -= 55.0f * dt;                  // float upwards
    }
}

// ----------------------------------------------------------------------------
//  Game rules
// ----------------------------------------------------------------------------
static int ComboMultiplier()
{
    int m = 1 + g_combo / 5;
    return (m > 4) ? 4 : m;
}

static void GameOver()
{
    g_state = STATE_GAMEOVER;
    g_combo = 0;
    if (g_score > g_highScore)
    {
        g_highScore = g_score;
        g_newHigh = true;
        SaveHighScore();
    }
    PlaySfx(SFX_GAMEOVER);
}

static void LoseLife()
{
    g_lives--;
    g_combo = 0;
    if (g_lives <= 0)
    {
        g_lives = 0;
        GameOver();
    }
}

static void OnCatch(Star& s)
{
    wchar_t buf[24];
    s.alive = false;

    if (s.type == STAR_BAD)
    {
        Burst(s.x, s.y, 26, RGB(235, 40, 70), RGB(120, 60, 180), 300.0f, 4);
        AddText(s.x, s.y - 20, L"-1 life", RGB(255, 90, 110));
        PlaySfx(SFX_BAD);
        LoseLife();
        return;
    }

    int points = ((s.type == STAR_GOLD) ? 5 : 1) * ComboMultiplier();
    g_score += points;
    g_combo++;
    if (g_combo > g_bestCombo) g_bestCombo = g_combo;
    g_goodCaught++;

    if (s.type == STAR_GOLD)
    {
        Burst(s.x, s.y, 34, RGB(255, 250, 170), RGB(255, 170, 40), 380.0f, 5);
        wsprintf(buf, L"+%d  GOLD!", points);
        AddText(s.x, s.y - 20, buf, RGB(255, 235, 90));
        PlaySfx(SFX_GOLD);
    }
    else
    {
        Burst(s.x, s.y, 16, RGB(255, 225, 70), RGB(255, 255, 200), 260.0f, 4);
        wsprintf(buf, L"+%d", points);
        AddText(s.x, s.y - 20, buf, RGB(255, 255, 255));
        PlaySfx(SFX_CATCH);
    }

    // Bonus life for a long streak
    if (g_combo % COMBO_FOR_LIFE == 0 && g_lives < MAX_LIVES)
    {
        g_lives++;
        AddText(g_player.x, g_player.y - 200, L"+1 life!", RGB(255, 120, 150));
    }

    // Level up
    int newLevel = 1 + g_goodCaught / STARS_PER_LEVEL;
    if (newLevel > g_level)
    {
        g_level = newLevel;
        wsprintf(buf, L"LEVEL %d", g_level);
        AddText(g_clientW / 2.0f - 60.0f, g_clientH * 0.35f, buf, RGB(120, 230, 255));
        PlaySfx(SFX_LEVELUP);
    }
}

static void OnMiss(Star& s)
{
    s.alive = false;
    Burst(s.x, GroundY(), 8, RGB(190, 190, 190), RGB(140, 140, 140), 140.0f, 3);
    AddText(s.x - 20, GroundY() - 40, L"Miss!", RGB(255, 110, 110));
    PlaySfx(SFX_MISS);
    LoseLife();
}

// ----------------------------------------------------------------------------
//  Stars
// ----------------------------------------------------------------------------
static void SpawnStar(bool decorative)
{
    for (int i = 0; i < MAX_STARS; i++)
    {
        Star& s = g_stars[i];
        if (s.alive) continue;

        float roll = RandF(0.0f, 1.0f);
        float badChance = (g_level >= 2) ? (0.12f + 0.01f * g_level) : 0.0f;
        if (badChance > 0.26f) badChance = 0.26f;

        if (decorative)
            s.type = (roll < 0.15f) ? STAR_GOLD : STAR_NORMAL;
        else if (roll < badChance)
            s.type = STAR_BAD;
        else if (roll < badChance + 0.10f)
            s.type = STAR_GOLD;
        else
            s.type = STAR_NORMAL;

        float speed = 140.0f + 24.0f * (g_level - 1);
        if (speed > 440.0f) speed = 440.0f;
        if (decorative) speed = 110.0f;

        s.alive = true;
        s.r = (s.type == STAR_GOLD) ? 30.0f : (s.type == STAR_BAD ? 24.0f : 21.0f);
        s.vy = speed * RandF(0.8f, 1.25f) * ((s.type == STAR_GOLD) ? 1.3f : 1.0f);
        s.baseX = RandF(s.r + 20.0f, g_clientW - s.r - 20.0f);
        s.x = s.baseX;
        s.y = -s.r;
        s.angle = RandF(0.0f, 2.0f * PI);
        s.spin = RandF(-2.0f, 2.0f);
        s.age = 0.0f;
        s.swayAmp = (s.type == STAR_GOLD) ? RandF(25.0f, 55.0f) : RandF(0.0f, 18.0f);
        s.swayFreq = RandF(1.5f, 3.0f);
        s.swayPhase = RandF(0.0f, 2.0f * PI);

        // keep the swaying star inside the window
        if (s.baseX - s.swayAmp < s.r) s.baseX = s.r + s.swayAmp;
        if (s.baseX + s.swayAmp > g_clientW - s.r) s.baseX = g_clientW - s.r - s.swayAmp;
        return;
    }
}

static void UpdateStars(float dt)
{
    const Player& p = g_player;

    for (int i = 0; i < MAX_STARS; i++)
    {
        Star& s = g_stars[i];
        if (!s.alive) continue;

        s.age += dt;
        s.y += s.vy * dt;
        s.x = s.baseX + sinf(s.age * s.swayFreq + s.swayPhase) * s.swayAmp;
        s.angle += s.spin * dt;

        if (g_state == STATE_PLAYING)
        {
            // Does the star touch the catching area? (circle vs rectangle)
            float left   = p.x - CATCH_HALF_W, right  = p.x + CATCH_HALF_W;
            float top    = p.y - CATCH_TOP,    bottom = p.y - CATCH_BOTTOM;
            float cx = (s.x < left) ? left : (s.x > right ? right : s.x);
            float cy = (s.y < top)  ? top  : (s.y > bottom ? bottom : s.y);
            float dx = s.x - cx, dy = s.y - cy;

            if (dx * dx + dy * dy <= s.r * s.r)
            {
                OnCatch(s);
                continue;
            }

            // Reached the ground?
            if (s.y >= GroundY() - 6.0f)
            {
                if (s.type == STAR_BAD)
                    s.alive = false;        // dodged it: no penalty
                else
                    OnMiss(s);
                continue;
            }
        }
        else if (s.y > g_clientH + 40.0f)
        {
            s.alive = false;                // decorative stars just fall away
        }
    }
}

// ----------------------------------------------------------------------------
//  Player
// ----------------------------------------------------------------------------
static void ClampPlayerX(float& x)
{
    float halfW = FRAME_W * SPRITE_SCALE / 2.0f;
    if (x < halfW) x = halfW;
    if (x > g_clientW - halfW) x = g_clientW - halfW;
}

static void UpdatePlayer(float dt)
{
    Player& p = g_player;
    p.y = GroundY();

    bool active = (GetForegroundWindow() == g_hWnd);
    float input = 0.0f;
    if (active)
    {
        if (KeyDown(VK_LEFT)  || KeyDown('A')) input -= 1.0f;
        if (KeyDown(VK_RIGHT) || KeyDown('D')) input += 1.0f;
    }
    p.running = active && KeyDown(VK_SHIFT);
    float speed = p.running ? RUN_SPEED : WALK_SPEED;

    float move = 0.0f;          // -1 .. +1
    if (input != 0.0f)
    {
        p.hasTarget = false;
        move = input;
    }
    else if (p.hasTarget)
    {
        float diff = p.targetX - p.x;
        if (fabsf(diff) <= speed * dt)
        {
            p.x = p.targetX;
            p.hasTarget = false;
        }
        else
        {
            move = (diff < 0.0f) ? -1.0f : 1.0f;
        }
    }

    p.moving = (move != 0.0f);
    if (p.moving)
    {
        p.x += move * speed * dt;
        ClampPlayerX(p.x);
        p.dir = (move < 0.0f) ? DIR_LEFT : DIR_RIGHT;
    }

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
        p.frame = 0;
        p.animTimer = 0.0f;
    }
}

// ----------------------------------------------------------------------------
//  Game flow
// ----------------------------------------------------------------------------
void StartGame()
{
    for (int i = 0; i < MAX_STARS; i++)     g_stars[i].alive = false;
    for (int i = 0; i < MAX_PARTICLES; i++) g_particles[i].alive = false;
    for (int i = 0; i < MAX_TEXTS; i++)     g_texts[i].alive = false;

    g_score = 0;
    g_lives = MAX_LIVES;
    g_combo = 0;
    g_bestCombo = 0;
    g_level = 1;
    g_goodCaught = 0;
    g_newHigh = false;
    g_spawnTimer = 0.8f;

    g_player.x = g_clientW / 2.0f;
    g_player.y = GroundY();
    g_player.dir = DIR_DOWN;
    g_player.frame = 0;
    g_player.animTimer = 0.0f;
    g_player.moving = false;
    g_player.hasTarget = false;

    g_state = STATE_PLAYING;
}

void TogglePause()
{
    if (g_state == STATE_PLAYING)      g_state = STATE_PAUSED;
    else if (g_state == STATE_PAUSED)  g_state = STATE_PLAYING;
}

void Update(float dt)
{
    g_time += dt;
    if (g_clientW <= 0 || g_clientH <= 0) return;

    switch (g_state)
    {
    case STATE_PLAYING:
    {
        UpdatePlayer(dt);

        float interval = 1.05f - 0.07f * (g_level - 1);
        if (interval < 0.32f) interval = 0.32f;

        g_spawnTimer -= dt;
        if (g_spawnTimer <= 0.0f)
        {
            SpawnStar(false);
            g_spawnTimer = interval * RandF(0.7f, 1.3f);
        }
        UpdateStars(dt);
        UpdateEffects(dt);
        break;
    }

    case STATE_TITLE:
    case STATE_GAMEOVER:
        // Calm decorative stars drift down behind the menu
        g_spawnTimer -= dt;
        if (g_spawnTimer <= 0.0f)
        {
            SpawnStar(true);
            g_spawnTimer = RandF(0.35f, 0.7f);
        }
        UpdateStars(dt);
        UpdateEffects(dt);
        break;

    case STATE_PAUSED:
        break;
    }
}

// ----------------------------------------------------------------------------
//  Drawing helpers
// ----------------------------------------------------------------------------
static void DrawStarShape(float cx, float cy, float R, float angle, float innerRatio,
                          HPEN pen, HBRUSH brush)
{
    POINT pts[10];
    for (int i = 0; i < 10; i++)
    {
        float r = (i % 2 == 0) ? R : R * innerRatio;
        float a = angle + i * PI / 5.0f - PI / 2.0f;
        pts[i].x = (LONG)(cx + cosf(a) * r);
        pts[i].y = (LONG)(cy + sinf(a) * r);
    }
    HGDIOBJ oldPen = SelectObject(g_backDC, pen);
    HGDIOBJ oldBr  = SelectObject(g_backDC, brush);
    Polygon(g_backDC, pts, 10);
    SelectObject(g_backDC, oldBr);
    SelectObject(g_backDC, oldPen);
}

static void DrawStar(const Star& s)
{
    float cx = s.x, cy = s.y;

    switch (s.type)
    {
    case STAR_NORMAL:
        DrawStarShape(cx, cy, s.r, s.angle, 0.5f, g_penNormal, g_brNormal);
        DrawStarShape(cx, cy, s.r * 0.45f, s.angle, 0.5f,
                      (HPEN)GetStockObject(NULL_PEN), (HBRUSH)GetStockObject(WHITE_BRUSH));
        break;

    case STAR_GOLD:
    {
        float pulse = 1.0f + 0.1f * sinf(s.age * 9.0f);
        DrawStarShape(cx, cy, s.r * pulse, s.angle, 0.5f, g_penGold, g_brGold);
        DrawStarShape(cx, cy, s.r * 0.5f * pulse, s.angle, 0.5f,
                      (HPEN)GetStockObject(NULL_PEN), (HBRUSH)GetStockObject(WHITE_BRUSH));
        break;
    }

    case STAR_BAD:
        DrawStarShape(cx, cy, s.r, s.angle, 0.38f, g_penBad, g_brBad);
        // glowing red core
        {
            HGDIOBJ oldPen = SelectObject(g_backDC, GetStockObject(NULL_PEN));
            HBRUSH core = CreateSolidBrush(RGB(255, 60, 90));
            HGDIOBJ oldBr = SelectObject(g_backDC, core);
            int cr = (int)(s.r * 0.22f);
            Ellipse(g_backDC, (int)cx - cr, (int)cy - cr, (int)cx + cr, (int)cy + cr);
            SelectObject(g_backDC, oldBr);
            SelectObject(g_backDC, oldPen);
            DeleteObject(core);
        }
        break;
    }
}

static void DrawHeart(int cx, int cy, float scale, bool filled)
{
    POINT pts[28];
    for (int i = 0; i < 28; i++)
    {
        float t = 2.0f * PI * i / 28.0f;
        float st = sinf(t);
        float x = 16.0f * st * st * st;
        float y = -(13.0f * cosf(t) - 5.0f * cosf(2 * t) - 2.0f * cosf(3 * t) - cosf(4 * t));
        pts[i].x = (LONG)(cx + x * scale);
        pts[i].y = (LONG)(cy + y * scale);
    }
    HGDIOBJ oldPen = SelectObject(g_backDC, filled ? g_penHeart : g_penGrey);
    HGDIOBJ oldBr  = SelectObject(g_backDC, filled ? (HGDIOBJ)g_brHeart
                                                   : GetStockObject(NULL_BRUSH));
    Polygon(g_backDC, pts, 28);
    SelectObject(g_backDC, oldBr);
    SelectObject(g_backDC, oldPen);
}

// Darkens a rectangle (see-through black)
static void DimRect(int l, int t, int r, int b, int alpha)
{
    HDC mem = CreateCompatibleDC(g_backDC);
    HBITMAP bmp = CreateCompatibleBitmap(g_backDC, 1, 1);
    HGDIOBJ old = SelectObject(mem, bmp);
    PatBlt(mem, 0, 0, 1, 1, BLACKNESS);

    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = (BYTE)alpha;
    bf.AlphaFormat = 0;
    AlphaBlend(g_backDC, l, t, r - l, b - t, mem, 0, 0, 1, 1, bf);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void DrawTextAt(HFONT font, int x, int y, const wchar_t* text, COLORREF color)
{
    int len = (int)wcslen(text);
    SelectObject(g_backDC, font);
    SetTextColor(g_backDC, RGB(0, 0, 0));
    TextOut(g_backDC, x + 2, y + 2, text, len);
    SetTextColor(g_backDC, color);
    TextOut(g_backDC, x, y, text, len);
}

static void DrawTextCentered(HFONT font, int y, const wchar_t* text, COLORREF color)
{
    SIZE size;
    SelectObject(g_backDC, font);
    GetTextExtentPoint32(g_backDC, text, (int)wcslen(text), &size);
    DrawTextAt(font, (g_clientW - size.cx) / 2, y, text, color);
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
        RECT r = { left, top, left + drawW, top + drawH };
        FillRect(g_backDC, &r, (HBRUSH)GetStockObject(GRAY_BRUSH));
        return;
    }

    HDC memDC = CreateCompatibleDC(g_backDC);
    HGDIOBJ old = SelectObject(memDC, sheet);
    int srcX = p.frame * FRAME_W;

    SetStretchBltMode(g_backDC, COLORONCOLOR);
    // mask first (AND), then the coloured sprite (OR) = transparent background
    StretchBlt(g_backDC, left, top, drawW, drawH, memDC, srcX, FRAME_H, FRAME_W, FRAME_H, SRCAND);
    StretchBlt(g_backDC, left, top, drawW, drawH, memDC, srcX, 0, FRAME_W, FRAME_H, SRCPAINT);

    SelectObject(memDC, old);
    DeleteDC(memDC);
}

static void DrawEffects()
{
    // particles
    HGDIOBJ oldBr = SelectObject(g_backDC, GetStockObject(DC_BRUSH));
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const Particle& p = g_particles[i];
        if (!p.alive) continue;
        int size = p.size;
        if (p.life < p.maxLife * 0.3f) size = (size > 2) ? size - 1 : size;
        SetDCBrushColor(g_backDC, p.color);
        PatBlt(g_backDC, (int)p.x, (int)p.y, size, size, PATCOPY);
    }
    SelectObject(g_backDC, oldBr);

    // floating texts
    for (int i = 0; i < MAX_TEXTS; i++)
    {
        const FloatText& t = g_texts[i];
        if (!t.alive) continue;
        DrawTextAt(g_fontMed, (int)t.x, (int)t.y, t.text, t.color);
    }
}

static void DrawHUD()
{
    wchar_t buf[96];

    DimRect(0, 0, g_clientW, 50, 120);

    wsprintf(buf, L"Score: %d", g_score);
    DrawTextAt(g_fontMed, 14, 9, buf, RGB(255, 255, 255));

    wsprintf(buf, L"Best: %d", g_highScore > g_score ? g_highScore : g_score);
    DrawTextAt(g_fontSmall, 220, 16, buf, RGB(255, 225, 120));

    wsprintf(buf, L"Level %d", g_level);
    DrawTextCentered(g_fontMed, 9, buf, RGB(130, 230, 255));

    // lives as hearts, right-aligned
    int x0 = g_clientW - 14 - MAX_LIVES * 34;
    for (int i = 0; i < MAX_LIVES; i++)
    {
        DrawHeart(x0 + i * 34 + 17, 26, 0.9f, i < g_lives);
    }

    // combo
    if (g_combo >= 3)
    {
        wsprintf(buf, L"Combo %d   x%d", g_combo, ComboMultiplier());
        DrawTextAt(g_fontMed, 14, 58, buf, RGB(255, 170, 60));
    }

    // tiny status line
    wsprintf(buf, L"FPS %d    Music: %s    Sound: %s",
             g_fps, !g_musicOpen ? L"(no file)" : (g_musicOn ? L"on" : L"off"),
             g_sfxOn ? L"on" : L"off");
    DrawTextAt(g_fontSmall, 10, g_clientH - 24, buf, RGB(230, 230, 230));
}

static void DrawLegend(int y)
{
    // example stars with captions
    int cx = g_clientW / 2;
    DrawStarShape((float)(cx - 280), (float)(y + 14), 18, 0, 0.5f, g_penNormal, g_brNormal);
    DrawTextAt(g_fontSmall, cx - 250, y + 4, L"+1 point", RGB(255, 255, 255));

    DrawStarShape((float)(cx - 60), (float)(y + 14), 22, 0, 0.5f, g_penGold, g_brGold);
    DrawTextAt(g_fontSmall, cx - 30, y + 4, L"+5 GOLD", RGB(255, 235, 90));

    DrawStarShape((float)(cx + 150), (float)(y + 14), 20, 0, 0.38f, g_penBad, g_brBad);
    DrawTextAt(g_fontSmall, cx + 180, y + 4, L"Avoid!", RGB(255, 110, 130));
}

static void DrawOverlay()
{
    wchar_t buf[96];
    int cy = g_clientH / 2;

    if (g_state == STATE_TITLE)
    {
        DimRect(0, 0, g_clientW, g_clientH, 150);
        DrawTextCentered(g_fontBig, cy - 250, L"STAR CATCHER", RGB(255, 225, 80));
        DrawTextCentered(g_fontMed, cy - 150, L"Catch the falling stars!", RGB(255, 255, 255));

        DrawLegend(cy - 90);

        DrawTextCentered(g_fontSmall, cy - 30,
            L"Move: Left / Right or A / D        Run: Shift        Click: walk to a spot",
            RGB(235, 235, 235));
        DrawTextCentered(g_fontSmall, cy - 4,
            L"Missing a star or catching a purple one costs a life.  Build combos for bonus points!",
            RGB(235, 235, 235));
        DrawTextCentered(g_fontSmall, cy + 22,
            L"P: pause     M: music     N: sound effects     Esc: quit",
            RGB(235, 235, 235));

        wsprintf(buf, L"High score: %d", g_highScore);
        DrawTextCentered(g_fontMed, cy + 70, buf, RGB(255, 225, 120));

        // blinking prompt
        if (((int)(g_time * 2.0f)) % 2 == 0)
        {
            DrawTextCentered(g_fontMed, cy + 130, L"Press ENTER or click to start",
                             RGB(130, 230, 255));
        }
    }
    else if (g_state == STATE_PAUSED)
    {
        DimRect(0, 0, g_clientW, g_clientH, 130);
        DrawTextCentered(g_fontBig, cy - 80, L"PAUSED", RGB(255, 255, 255));
        DrawTextCentered(g_fontMed, cy + 20, L"Press P or right-click to continue",
                         RGB(130, 230, 255));
    }
    else if (g_state == STATE_GAMEOVER)
    {
        DimRect(0, 0, g_clientW, g_clientH, 160);
        DrawTextCentered(g_fontBig, cy - 220, L"GAME OVER", RGB(255, 100, 120));

        wsprintf(buf, L"Score: %d", g_score);
        DrawTextCentered(g_fontMed, cy - 110, buf, RGB(255, 255, 255));

        wsprintf(buf, L"Level reached: %d        Best combo: %d", g_level, g_bestCombo);
        DrawTextCentered(g_fontSmall, cy - 60, buf, RGB(235, 235, 235));

        if (g_newHigh)
        {
            COLORREF c = (((int)(g_time * 4.0f)) % 2 == 0) ? RGB(255, 235, 90)
                                                           : RGB(255, 160, 40);
            DrawTextCentered(g_fontMed, cy - 10, L"NEW HIGH SCORE!", c);
        }
        else
        {
            wsprintf(buf, L"High score: %d", g_highScore);
            DrawTextCentered(g_fontMed, cy - 10, buf, RGB(255, 225, 120));
        }

        if (((int)(g_time * 2.0f)) % 2 == 0)
        {
            DrawTextCentered(g_fontMed, cy + 80, L"Press ENTER or click to play again",
                             RGB(130, 230, 255));
        }
    }
}

void RenderFrame()
{
    if (g_backDC == NULL) return;

    BitBlt(g_backDC, 0, 0, g_clientW, g_clientH, g_bgDC, 0, 0, SRCCOPY);

    for (int i = 0; i < MAX_STARS; i++)
    {
        if (g_stars[i].alive) DrawStar(g_stars[i]);
    }

    DrawPlayer();
    DrawEffects();

    if (g_state != STATE_TITLE)
    {
        DrawHUD();
    }
    DrawOverlay();
}

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
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RenderFrame();
        Present(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            RebuildBuffers(hWnd);
            ClampPlayerX(g_player.x);
            g_player.y = GroundY();
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_state == STATE_TITLE || g_state == STATE_GAMEOVER)
        {
            StartGame();
        }
        else if (g_state == STATE_PLAYING)
        {
            g_player.targetX = (float)GET_X_LPARAM(lParam);
            ClampPlayerX(g_player.targetX);
            g_player.hasTarget = true;
        }
        return 0;

    case WM_RBUTTONDOWN:
        TogglePause();
        return 0;

    case WM_KEYDOWN:
        if (lParam & 0x40000000)        // ignore auto-repeat
        {
            return 0;
        }
        switch (wParam)
        {
        case VK_ESCAPE:
            DestroyWindow(hWnd);
            break;
        case VK_RETURN:
        case VK_SPACE:
            if (g_state == STATE_TITLE || g_state == STATE_GAMEOVER) StartGame();
            break;
        case 'P':
            TogglePause();
            break;
        case 'M':
            SetMusicOn(!g_musicOn);
            break;
        case 'N':
            g_sfxOn = !g_sfxOn;
            break;
        }
        return 0;

    case WM_KILLFOCUS:
        // Pause automatically when the player switches to another window
        if (g_state == STATE_PLAYING) g_state = STATE_PAUSED;
        return 0;

    case MM_MCINOTIFY:
        if (wParam == MCI_NOTIFY_SUCCESSFUL && g_musicOpen && g_musicOn)
        {
            mciSendString(L"seek bgm to start", NULL, 0, NULL);
            mciSendString(L"play bgm notify", NULL, 0, hWnd);
        }
        return 0;

    case WM_DESTROY:
        PlaySound(NULL, NULL, 0);
        StopMusic();
        ReleaseBuffers();
        ReleaseImages();
        ReleaseGraphics();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}
