#include <windows.h>
HPEN myPen;
HBRUSH myBrush;

int playerX = 90;
int playerY = 100;

int playerSpeed = 10;

//next i should work on #include <stdio.h>
LRESULT CALLBACK WndProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC hdc = BeginPaint(hWnd, &ps);

			SelectObject(hdc, myPen);
			/*
            TextOutW(
                hdc,
                100,
                100,
                L"Hello from my window!",
                21
            );*/


			MoveToEx(

				hdc,
				100,
				100,
				NULL
				);

			LineTo(
				hdc,
			    500,
				500
				);

			SelectObject(hdc, myBrush);
		    
			Rectangle(
				hdc,
				playerX,
				playerY,
			    playerX + 250,
				playerY + 200
				);

            EndPaint(hWnd, &ps);

            break;
        }

		case WM_KEYDOWN:
			{
			if(wParam == VK_LEFT)
			{
				playerX -= playerSpeed;
			}

			if(wParam = VK_RIGHT)
			{
				playerX += playerSpeed;
			}
			if(wParam == VK_UP)
			{
				playerY -= playerSpeed;

			}

			if(wParam == VK_DOWN){
				playerY += playerSpeed;
			}
			InvalidateRect(hWnd, NULL, TRUE);
			
			break;
			}


		case WM_DESTROY:
			{
				PostQuitMessage(0);
				break;
			}


        default:
            return DefWindowProcW(
                hWnd,
                message,
                wParam,
                lParam
            );
    }

    return 0;
}
int WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR lpCmdLine, 
	int nCmdShow
	)

{
    WNDCLASSEXW wc = {};

    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = hInstance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"MyGameWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassExW(&wc);

	myPen = CreatePen(
		PS_SOLID,
		3,
		RGB(255, 0, 0)
		);

	myBrush = CreateSolidBrush(
		RGB(0, 255, 0)
		);

    HWND hWnd = CreateWindowExW(
        0,
        L"MyGameWindow",
        L"My First Window",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        600,
        400,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    ShowWindow(hWnd, nCmdShow);

	MSG msg;
	
	while(GetMessageW(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    	DeleteObject(myPen);
		DeleteObject(myBrush);

    return 0;
}