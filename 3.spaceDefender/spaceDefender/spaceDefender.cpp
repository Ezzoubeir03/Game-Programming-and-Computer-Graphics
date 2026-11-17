
#include "stdafx.h"
#include <windows.h>
#include <tchar.h>

// Function defined in space.cpp
LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    LPCTSTR className = _T("SpaceDefenderWindow");

    // Register window class
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(
            NULL,
            _T("Window registration failed!"),
            _T("Space Defender"),
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    // Set client window size
    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = 800;
    rect.bottom = 650;

    AdjustWindowRect(
        &rect,
        WS_OVERLAPPEDWINDOW,
        FALSE
    );

    // Create game window
    HWND hwnd = CreateWindowEx(
        0,
        className,
        _T("SPACE DEFENDER - Protect the Galaxy"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL)
    {
        MessageBox(
            NULL,
            _T("Could not create the game window!"),
            _T("Space Defender"),
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    // Display game window
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main Windows message loop
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
