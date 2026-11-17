
#include <windows.h>
#include <cstdlib>
#include <ctime>

// Window name
const wchar_t CLASS_NAME[] = L"GoGameWindow";

// Go board settings
const int BOARD_SIZE = 9;

// Board values:
// 0 = Empty
// 1 = Black stone (Player)
// 2 = White stone (Computer)

int board[9][9] = { 0 };

// Game settings
bool gameStarted = false;
bool gameOver = false;
bool playerTurn = true;

// Consecutive passes
int consecutivePasses = 0;

// Captured stones
int blackCaptured = 0;
int whiteCaptured = 0;

// Territory scores
int blackTerritoryScore = 0;
int whiteTerritoryScore = 0;

// Final scores
double blackScore = 0;
double whiteScore = 0;

// White receives 6.5 komi points
const double KOMI = 6.5;

// Game status
const wchar_t* gameStatus = L"Press START to begin playing";

// Board image position and size
const int BOARD_X = 30;
const int BOARD_Y = 125;
const int BOARD_WIDTH = 620;
const int BOARD_HEIGHT = 620;

// Original board image dimensions
const int ORIGINAL_BOARD_SIZE = 544;

// Grid positions inside the original image
const int GRID_START = 48;
const int GRID_STEP = 56;

// Stone display size
const int STONE_SIZE = 42;

// Window image structure
struct GameImage
{
    HBITMAP bitmap;
    int width;
    int height;
};

// Game images
GameImage backgroundImage = { NULL, 0, 0 };
GameImage boardImage = { NULL, 0, 0 };
GameImage blackStoneImage = { NULL, 0, 0 };
GameImage whiteStoneImage = { NULL, 0, 0 };
GameImage playerImage = { NULL, 0, 0 };
GameImage computerImage = { NULL, 0, 0 };
GameImage logoImage = { NULL, 0, 0 };
GameImage startButtonImage = { NULL, 0, 0 };
GameImage restartButtonImage = { NULL, 0, 0 };
GameImage passButtonImage = { NULL, 0, 0 };
GameImage exitButtonImage = { NULL, 0, 0 };

// Load a BMP image
bool LoadGameImage(GameImage& image, const wchar_t* filename)
{
    image.bitmap = (HBITMAP)LoadImageW(
        NULL,
        filename,
        IMAGE_BITMAP,
        0,
        0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION
    );

    if (image.bitmap == NULL)
    {
        return false;
    }

    BITMAP bitmapInfo;

    GetObjectW(
        image.bitmap,
        sizeof(BITMAP),
        &bitmapInfo
    );

    image.width = bitmapInfo.bmWidth;
    image.height = bitmapInfo.bmHeight;

    return true;
}

// Display a bitmap image
void DrawGameImage(
    HDC hdc,
    GameImage& image,
    int x,
    int y,
    int width,
    int height
)
{
    if (image.bitmap == NULL)
    {
        return;
    }

    HDC memoryDC = CreateCompatibleDC(hdc);

    if (memoryDC == NULL)
    {
        return;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(
        memoryDC,
        image.bitmap
    );

    int oldMode = SetStretchBltMode(hdc, HALFTONE);

    StretchBlt(
        hdc,
        x,
        y,
        width,
        height,
        memoryDC,
        0,
        0,
        image.width,
        image.height,
        SRCCOPY
    );

    SetStretchBltMode(hdc, oldMode);

    SelectObject(memoryDC, oldBitmap);
    DeleteDC(memoryDC);
}

// Display text
void DrawGameText(
    HDC hdc,
    const wchar_t* text,
    int x,
    int y,
    int width,
    int height,
    int fontSize,
    COLORREF color
)
{
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
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        L"Arial"
    );

    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    RECT rect = {
        x,
        y,
        x + width,
        y + height
    };

    DrawTextW(
        hdc,
        text,
        -1,
        &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// Load all images
void LoadAllImages()
{
    LoadGameImage(backgroundImage, L"Images\\background.bmp");
    LoadGameImage(boardImage, L"Images\\go_board.bmp");
    LoadGameImage(blackStoneImage, L"Images\\black_stone.bmp");
    LoadGameImage(whiteStoneImage, L"Images\\white_stone.bmp");
    LoadGameImage(playerImage, L"Images\\player.bmp");
    LoadGameImage(computerImage, L"Images\\computer.bmp");
    LoadGameImage(logoImage, L"Images\\logo.bmp");
    LoadGameImage(startButtonImage, L"Images\\start_button.bmp");
    LoadGameImage(restartButtonImage, L"Images\\restart_button.bmp");
    LoadGameImage(passButtonImage, L"Images\\pass_button.bmp");
    LoadGameImage(exitButtonImage, L"Images\\exit_button.bmp");
}

// Release all images
void ReleaseAllImages()
{
    GameImage* images[] =
    {
        &backgroundImage,
        &boardImage,
        &blackStoneImage,
        &whiteStoneImage,
        &playerImage,
        &computerImage,
        &logoImage,
        &startButtonImage,
        &restartButtonImage,
        &passButtonImage,
        &exitButtonImage
    };

    for (int i = 0; i < 11; i++)
    {
        if (images[i]->bitmap != NULL)
        {
            DeleteObject(images[i]->bitmap);
            images[i]->bitmap = NULL;
        }
    }
}

// Get X coordinate of a board intersection
int GetIntersectionX(int column)
{
    int originalWidth = ORIGINAL_BOARD_SIZE;

    if (boardImage.width > 0)
    {
        originalWidth = boardImage.width;
    }

    int originalPosition = GRID_START + column * GRID_STEP;

    return BOARD_X +
        originalPosition * BOARD_WIDTH / originalWidth;
}

// Get Y coordinate of a board intersection
int GetIntersectionY(int row)
{
    int originalHeight = ORIGINAL_BOARD_SIZE;

    if (boardImage.height > 0)
    {
        originalHeight = boardImage.height;
    }

    int originalPosition = GRID_START + row * GRID_STEP;

    return BOARD_Y +
        originalPosition * BOARD_HEIGHT / originalHeight;
}

// Check whether a position is inside a rectangle
bool IsInsideButton(
    int x,
    int y,
    int left,
    int top,
    int right,
    int bottom
)
{
    return (
        x >= left &&
        x <= right &&
        y >= top &&
        y <= bottom
    );
}

// Get a connected group and count its liberties
void GetGroup(
    int startRow,
    int startColumn,
    int color,
    int groupRows[],
    int groupColumns[],
    int& groupSize,
    int& liberties
)
{
    bool visited[9][9] = { false };
    bool visitedLiberties[9][9] = { false };

    int stackRows[81];
    int stackColumns[81];

    int stackSize = 0;

    groupSize = 0;
    liberties = 0;

    stackRows[stackSize] = startRow;
    stackColumns[stackSize] = startColumn;

    stackSize++;

    visited[startRow][startColumn] = true;

    int directions[4][2] =
    {
        { -1, 0 },
        { 1, 0 },
        { 0, -1 },
        { 0, 1 }
    };

    while (stackSize > 0)
    {
        stackSize--;

        int row = stackRows[stackSize];
        int column = stackColumns[stackSize];

        groupRows[groupSize] = row;
        groupColumns[groupSize] = column;

        groupSize++;

        for (int i = 0; i < 4; i++)
        {
            int newRow = row + directions[i][0];
            int newColumn = column + directions[i][1];

            if (
                newRow < 0 ||
                newRow >= BOARD_SIZE ||
                newColumn < 0 ||
                newColumn >= BOARD_SIZE
            )
            {
                continue;
            }

            // Count empty neighboring positions
            if (board[newRow][newColumn] == 0)
            {
                if (!visitedLiberties[newRow][newColumn])
                {
                    liberties++;

                    visitedLiberties[newRow][newColumn] = true;
                }
            }

            // Add connected stones of the same color
            else if (
                board[newRow][newColumn] == color &&
                !visited[newRow][newColumn]
            )
            {
                visited[newRow][newColumn] = true;

                stackRows[stackSize] = newRow;
                stackColumns[stackSize] = newColumn;

                stackSize++;
            }
        }
    }
}

// Place a stone and check for captures
bool TryPlaceStone(int row, int column, int color)
{
    if (
        row < 0 ||
        row >= BOARD_SIZE ||
        column < 0 ||
        column >= BOARD_SIZE
    )
    {
        return false;
    }

    if (board[row][column] != 0)
    {
        return false;
    }

    // Save board before making the move
    int oldBoard[9][9];

    for (int r = 0; r < BOARD_SIZE; r++)
    {
        for (int c = 0; c < BOARD_SIZE; c++)
        {
            oldBoard[r][c] = board[r][c];
        }
    }

    int oldBlackCaptured = blackCaptured;
    int oldWhiteCaptured = whiteCaptured;

    // Place the stone
    board[row][column] = color;

    int opponentColor;

    if (color == 1)
    {
        opponentColor = 2;
    }
    else
    {
        opponentColor = 1;
    }

    int directions[4][2] =
    {
        { -1, 0 },
        { 1, 0 },
        { 0, -1 },
        { 0, 1 }
    };

    // Check neighboring opponent groups
    for (int i = 0; i < 4; i++)
    {
        int newRow = row + directions[i][0];
        int newColumn = column + directions[i][1];

        if (
            newRow < 0 ||
            newRow >= BOARD_SIZE ||
            newColumn < 0 ||
            newColumn >= BOARD_SIZE
        )
        {
            continue;
        }

        if (board[newRow][newColumn] == opponentColor)
        {
            int groupRows[81];
            int groupColumns[81];

            int groupSize = 0;
            int liberties = 0;

            GetGroup(
                newRow,
                newColumn,
                opponentColor,
                groupRows,
                groupColumns,
                groupSize,
                liberties
            );

            // Remove groups with no liberties
            if (liberties == 0)
            {
                for (int j = 0; j < groupSize; j++)
                {
                    board[groupRows[j]][groupColumns[j]] = 0;
                }

                if (color == 1)
                {
                    blackCaptured += groupSize;
                }
                else
                {
                    whiteCaptured += groupSize;
                }
            }
        }
    }

    // Check the liberties of the newly placed group
    int ownRows[81];
    int ownColumns[81];

    int ownGroupSize = 0;
    int ownLiberties = 0;

    GetGroup(
        row,
        column,
        color,
        ownRows,
        ownColumns,
        ownGroupSize,
        ownLiberties
    );

    // Prevent suicide moves
    if (ownLiberties == 0)
    {
        for (int r = 0; r < BOARD_SIZE; r++)
        {
            for (int c = 0; c < BOARD_SIZE; c++)
            {
                board[r][c] = oldBoard[r][c];
            }
        }

        blackCaptured = oldBlackCaptured;
        whiteCaptured = oldWhiteCaptured;

        return false;
    }

    return true;
}

// Calculate territory and final scores
void CalculateScores()
{
    bool visited[9][9] = { false };

    blackTerritoryScore = 0;
    whiteTerritoryScore = 0;

    // Find empty regions on the board
    for (int row = 0; row < BOARD_SIZE; row++)
    {
        for (int column = 0; column < BOARD_SIZE; column++)
        {
            if (
                board[row][column] != 0 ||
                visited[row][column]
            )
            {
                continue;
            }

            int queueRows[81];
            int queueColumns[81];

            int queueStart = 0;
            int queueEnd = 0;

            int regionSize = 0;

            bool touchesBlack = false;
            bool touchesWhite = false;

            queueRows[queueEnd] = row;
            queueColumns[queueEnd] = column;

            queueEnd++;

            visited[row][column] = true;

            // Explore the empty region
            while (queueStart < queueEnd)
            {
                int currentRow = queueRows[queueStart];
                int currentColumn = queueColumns[queueStart];

                queueStart++;

                regionSize++;

                int directions[4][2] =
                {
                    { -1, 0 },
                    { 1, 0 },
                    { 0, -1 },
                    { 0, 1 }
                };

                for (int i = 0; i < 4; i++)
                {
                    int newRow = currentRow + directions[i][0];
                    int newColumn = currentColumn + directions[i][1];

                    if (
                        newRow < 0 ||
                        newRow >= BOARD_SIZE ||
                        newColumn < 0 ||
                        newColumn >= BOARD_SIZE
                    )
                    {
                        continue;
                    }

                    if (board[newRow][newColumn] == 0)
                    {
                        if (!visited[newRow][newColumn])
                        {
                            visited[newRow][newColumn] = true;

                            queueRows[queueEnd] = newRow;
                            queueColumns[queueEnd] = newColumn;

                            queueEnd++;
                        }
                    }
                    else if (board[newRow][newColumn] == 1)
                    {
                        touchesBlack = true;
                    }
                    else if (board[newRow][newColumn] == 2)
                    {
                        touchesWhite = true;
                    }
                }
            }

            // Assign territory
            if (touchesBlack && !touchesWhite)
            {
                blackTerritoryScore += regionSize;
            }
            else if (touchesWhite && !touchesBlack)
            {
                whiteTerritoryScore += regionSize;
            }
        }
    }

    // Calculate final scores
    // Territory + captured stones

    blackScore =
        blackTerritoryScore + blackCaptured;

    whiteScore =
        whiteTerritoryScore + whiteCaptured + KOMI;
}

// End the game
void EndGame(HWND hwnd)
{
    gameOver = true;
    gameStarted = false;

    playerTurn = false;

    CalculateScores();

    if (blackScore > whiteScore)
    {
        gameStatus = L"BLACK WINS! Congratulations, Player!";
    }
    else if (whiteScore > blackScore)
    {
        gameStatus = L"WHITE WINS! Computer wins!";
    }
    else
    {
        gameStatus = L"DRAW! Both players have equal scores.";
    }

    InvalidateRect(hwnd, NULL, FALSE);
}

// Restart the game
void ResetGame(HWND hwnd)
{
    for (int row = 0; row < BOARD_SIZE; row++)
    {
        for (int column = 0; column < BOARD_SIZE; column++)
        {
            board[row][column] = 0;
        }
    }

    blackCaptured = 0;
    whiteCaptured = 0;

    blackTerritoryScore = 0;
    whiteTerritoryScore = 0;

    blackScore = 0;
    whiteScore = 0;

    consecutivePasses = 0;

    gameOver = false;
    gameStarted = true;
    playerTurn = true;

    gameStatus = L"New game started! Your turn.";

    InvalidateRect(hwnd, NULL, FALSE);
}

// Computer makes a random legal move
bool ComputerMove()
{
    int startPosition = rand() % 81;

    // Try every board position
    for (int i = 0; i < 81; i++)
    {
        int position = (startPosition + i) % 81;

        int row = position / 9;
        int column = position % 9;

        if (TryPlaceStone(row, column, 2))
        {
            return true;
        }
    }

    return false;
}

// Handle player board clicks
void HandleBoardClick(int mouseX, int mouseY, HWND hwnd)
{
    if (!gameStarted || gameOver || !playerTurn)
    {
        return;
    }

    for (int row = 0; row < BOARD_SIZE; row++)
    {
        for (int column = 0; column < BOARD_SIZE; column++)
        {
            int x = GetIntersectionX(column);
            int y = GetIntersectionY(row);

            int toleranceX =
                BOARD_WIDTH * GRID_STEP /
                ORIGINAL_BOARD_SIZE / 3;

            int toleranceY =
                BOARD_HEIGHT * GRID_STEP /
                ORIGINAL_BOARD_SIZE / 3;

            if (
                abs(mouseX - x) <= toleranceX &&
                abs(mouseY - y) <= toleranceY
            )
            {
                // Try to place a black stone
                if (TryPlaceStone(row, column, 1))
                {
                    // A normal move breaks consecutive passes
                    consecutivePasses = 0;

                    playerTurn = false;

                    gameStatus = L"Computer is thinking...";

                    // Computer plays
                    bool computerPlayed = ComputerMove();

                    if (computerPlayed)
                    {
                        consecutivePasses = 0;

                        gameStatus = L"Your turn! Place a black stone.";
                    }
                    else
                    {
                        // Computer passes if it has no legal moves
                        consecutivePasses++;

                        if (consecutivePasses >= 2)
                        {
                            EndGame(hwnd);
                            return;
                        }

                        gameStatus = L"Computer passed. Your turn.";
                    }

                    playerTurn = true;

                    InvalidateRect(hwnd, NULL, FALSE);
                }
                else
                {
                    gameStatus = L"Invalid move! Choose another intersection.";

                    InvalidateRect(hwnd, NULL, FALSE);
                }

                return;
            }
        }
    }
}

// Player passes
void PlayerPass(HWND hwnd)
{
    if (!gameStarted || gameOver || !playerTurn)
    {
        return;
    }

    // Player passes
    consecutivePasses++;

    playerTurn = false;

    gameStatus = L"Player passed. Computer is thinking...";

    // Simple computer strategy:
    // Computer also passes when the player passes.
    consecutivePasses++;

    if (consecutivePasses >= 2)
    {
        EndGame(hwnd);
        return;
    }

    playerTurn = true;

    gameStatus = L"Computer passed. Your turn.";

    InvalidateRect(hwnd, NULL, FALSE);
}

// Draw the game interface
void DrawGame(HDC hdc, HWND hwnd)
{
    RECT rect;

    GetClientRect(hwnd, &rect);

    int windowWidth = rect.right;
    int windowHeight = rect.bottom;

    // Background
    if (backgroundImage.bitmap != NULL)
    {
        DrawGameImage(
            hdc,
            backgroundImage,
            0,
            0,
            windowWidth,
            windowHeight
        );
    }
    else
    {
        HBRUSH brush = CreateSolidBrush(
            RGB(240, 220, 170)
        );

        FillRect(hdc, &rect, brush);

        DeleteObject(brush);
    }

    // Game logo
    DrawGameImage(
        hdc,
        logoImage,
        30,
        20,
        100,
        90
    );

    // Game title
    DrawGameText(
        hdc,
        L"GO GAME - PLAYER VS COMPUTER",
        140,
        25,
        600,
        65,
        30,
        RGB(55, 35, 20)
    );

    // Go board
    DrawGameImage(
        hdc,
        boardImage,
        BOARD_X,
        BOARD_Y,
        BOARD_WIDTH,
        BOARD_HEIGHT
    );

    // Draw stones
    for (int row = 0; row < BOARD_SIZE; row++)
    {
        for (int column = 0; column < BOARD_SIZE; column++)
        {
            int x = GetIntersectionX(column);
            int y = GetIntersectionY(row);

            if (board[row][column] == 1)
            {
                DrawGameImage(
                    hdc,
                    blackStoneImage,
                    x - STONE_SIZE / 2,
                    y - STONE_SIZE / 2,
                    STONE_SIZE,
                    STONE_SIZE
                );
            }
            else if (board[row][column] == 2)
            {
                DrawGameImage(
                    hdc,
                    whiteStoneImage,
                    x - STONE_SIZE / 2,
                    y - STONE_SIZE / 2,
                    STONE_SIZE,
                    STONE_SIZE
                );
            }
        }
    }

    // Game information title
    DrawGameText(
        hdc,
        L"GAME INFORMATION",
        700,
        115,
        400,
        45,
        24,
        RGB(55, 35, 20)
    );

    // Player image
    DrawGameImage(
        hdc,
        playerImage,
        750,
        180,
        100,
        100
    );

    DrawGameText(
        hdc,
        L"PLAYER - BLACK",
        730,
        285,
        140,
        35,
        17,
        RGB(30, 30, 30)
    );

    // Computer image
    DrawGameImage(
        hdc,
        computerImage,
        930,
        180,
        100,
        100
    );

    DrawGameText(
        hdc,
        L"COMPUTER - WHITE",
        905,
        285,
        155,
        35,
        17,
        RGB(30, 30, 30)
    );

    // Captured stones
    DrawGameText(
        hdc,
        L"CAPTURED STONES",
        700,
        345,
        400,
        35,
        21,
        RGB(55, 35, 20)
    );

    wchar_t capturedText[100];

    wsprintfW(
        capturedText,
        L"Player: %d     Computer: %d",
        blackCaptured,
        whiteCaptured
    );

    DrawGameText(
        hdc,
        capturedText,
        700,
        380,
        400,
        30,
        17,
        RGB(30, 30, 30)
    );

    // Game status
    DrawGameText(
        hdc,
        gameStatus,
        690,
        425,
        430,
        45,
        16,
        gameOver ? RGB(160, 20, 20) : RGB(30, 90, 30)
    );

    // Final score information
    if (gameOver)
    {
        wchar_t scoreText[150];

        wsprintfW(
            scoreText,
            L"Black: %d + %d = %.1f",
            blackTerritoryScore,
            blackCaptured,
            blackScore
        );

        DrawGameText(
            hdc,
            scoreText,
            700,
            475,
            400,
            30,
            17,
            RGB(20, 20, 20)
        );

        wsprintfW(
            scoreText,
            L"White: %d + %d + 6.5 = %.1f",
            whiteTerritoryScore,
            whiteCaptured,
            whiteScore
        );

        DrawGameText(
            hdc,
            scoreText,
            700,
            510,
            400,
            30,
            17,
            RGB(20, 20, 20)
        );

        DrawGameText(
            hdc,
            L"GAME OVER",
            700,
            545,
            400,
            30,
            22,
            RGB(160, 20, 20)
        );
    }
    else
    {
        DrawGameText(
            hdc,
            L"Territory and final scores appear at game end.",
            700,
            475,
            400,
            30,
            14,
            RGB(55, 35, 20)
        );
    }

    // Start button
    DrawGameImage(
        hdc,
        startButtonImage,
        720,
        600,
        170,
        65
    );

    // Restart button
    DrawGameImage(
        hdc,
        restartButtonImage,
        920,
        600,
        170,
        65
    );

    // Pass button
    DrawGameImage(
        hdc,
        passButtonImage,
        720,
        700,
        170,
        65
    );

    // Exit button
    DrawGameImage(
        hdc,
        exitButtonImage,
        920,
        700,
        170,
        65
    );
}

// Window procedure
LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        LoadAllImages();

        return 0;
    }

    // Draw the game interface
    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC hdc = BeginPaint(hwnd, &ps);

        DrawGame(hdc, hwnd);

        EndPaint(hwnd, &ps);

        return 0;
    }

    // Handle mouse clicks
    case WM_LBUTTONDOWN:
    {
        int mouseX = LOWORD(lParam);
        int mouseY = HIWORD(lParam);

        // Start button
        if (
            IsInsideButton(
                mouseX,
                mouseY,
                720,
                600,
                890,
                665
            )
        )
        {
            if (!gameOver)
            {
                gameStarted = true;
                playerTurn = true;

                gameStatus = L"Your turn! Place a black stone.";

                InvalidateRect(hwnd, NULL, FALSE);
            }

            return 0;
        }

        // Restart button
        if (
            IsInsideButton(
                mouseX,
                mouseY,
                920,
                600,
                1090,
                665
            )
        )
        {
            ResetGame(hwnd);

            return 0;
        }

        // Pass button
        if (
            IsInsideButton(
                mouseX,
                mouseY,
                720,
                700,
                890,
                765
            )
        )
        {
            PlayerPass(hwnd);

            return 0;
        }

        // Exit button
        if (
            IsInsideButton(
                mouseX,
                mouseY,
                920,
                700,
                1090,
                765
            )
        )
        {
            DestroyWindow(hwnd);

            return 0;
        }

        // Board click
        HandleBoardClick(mouseX, mouseY, hwnd);

        return 0;
    }

    // Close application
    case WM_DESTROY:
    {
        ReleaseAllImages();

        PostQuitMessage(0);

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam
    );
}

// Main Windows application
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    // Initialize random numbers
    srand((unsigned int)time(NULL));

    // Window class
    WNDCLASSW wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    // Create game window
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Go Game - C++ Final Project",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1150,
        850,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    UpdateWindow(hwnd);

    // Windows message loop
    MSG msg = {};

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);

        DispatchMessageW(&msg);
    }

    return 0;
}
