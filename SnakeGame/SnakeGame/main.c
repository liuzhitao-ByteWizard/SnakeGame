#include "raylib.h"

#include <stdbool.h>

/*
    Phase 1 只负责创建窗口和静态界面布局。

    真正的游戏规则会在后续阶段加入，例如蛇的移动、食物生成、计分存档，
    以及用于表示蛇身的双向循环链表。先把第一阶段做简单，可以提前确认
    地图尺寸和信息栏布局是否正确；后续接入游戏核心时，就不需要一边调
    规则一边猜界面尺寸。
*/

#define GRID_COLUMNS 30
#define GRID_ROWS 20
#define CELL_SIZE 28

#define WINDOW_PADDING 24
#define BOARD_BORDER 3
#define PANEL_GAP 24
#define INFO_PANEL_WIDTH 260

#define BOARD_WIDTH (GRID_COLUMNS * CELL_SIZE)
#define BOARD_HEIGHT (GRID_ROWS * CELL_SIZE)
#define WINDOW_WIDTH (WINDOW_PADDING * 2 + BOARD_WIDTH + PANEL_GAP + INFO_PANEL_WIDTH)
#define WINDOW_HEIGHT (WINDOW_PADDING * 2 + BOARD_HEIGHT)

#define BOARD_X WINDOW_PADDING
#define BOARD_Y WINDOW_PADDING
#define PANEL_X (BOARD_X + BOARD_WIDTH + PANEL_GAP)
#define PANEL_Y WINDOW_PADDING

static const Color kWindowBackground = { 10, 14, 18, 255 };
static const Color kBoardBorderColor = { 52, 71, 62, 255 };
static const Color kGridDark = { 20, 29, 31, 255 };
static const Color kGridGreen = { 24, 42, 36, 255 };
static const Color kPanelBackground = { 18, 24, 29, 255 };
static const Color kPanelStroke = { 70, 89, 80, 255 };
static const Color kTitleColor = { 228, 246, 234, 255 };
static const Color kTextColor = { 190, 207, 197, 255 };
static const Color kMutedTextColor = { 123, 142, 134, 255 };
static const Color kAccentGreen = { 90, 210, 128, 255 };
static const Color kAppleRed = { 224, 66, 73, 255 };

/*
    把一个“网格坐标”转换成 raylib 绘制时需要的“像素矩形”。

    项目中的游戏位置统一使用网格坐标，而不是屏幕像素：
    (0, 0) 表示左上角格子，x 向右增加，y 向下增加。
    只有界面层需要知道像素位置；游戏核心只应该关心蛇头在第几列第几行。
    做到这层拆分后，核心逻辑就不依赖 raylib，也不依赖窗口尺寸，后续更容易测试。
*/
static Rectangle GridCellToRectangle(int gridX, int gridY)
{
    Rectangle cell = {
        (float)(BOARD_X + gridX * CELL_SIZE),
        (float)(BOARD_Y + gridY * CELL_SIZE),
        (float)CELL_SIZE,
        (float)CELL_SIZE
    };

    return cell;
}

static void DrawBoardBackground(void)
{
    /* 
        棋盘使用 GRID_COLUMNS 和 GRID_ROWS 逐格绘制。
        后续游戏核心也会使用同样的 30 x 20 地图尺寸；Phase 1 先把棋盘画出来，
        可以直观看到界面是否已经符合规格。
    */
    DrawRectangle(
        BOARD_X - BOARD_BORDER,
        BOARD_Y - BOARD_BORDER,
        BOARD_WIDTH + BOARD_BORDER * 2,
        BOARD_HEIGHT + BOARD_BORDER * 2,
        kBoardBorderColor);

    for (int y = 0; y < GRID_ROWS; ++y)
    {
        for (int x = 0; x < GRID_COLUMNS; ++x)
        {
            const bool useGreenCell = ((x + y) % 2) == 0;
            const Color cellColor = useGreenCell ? kGridGreen : kGridDark;
            const Rectangle cell = GridCellToRectangle(x, y);

            DrawRectangleRec(cell, cellColor);
        }
    }

    /*
        轻微的网格线可以帮助观察每个格子的边界。
        线条颜色刻意压暗，避免后续绘制蛇和苹果时被网格线抢走注意力。
    */
    for (int x = 0; x <= GRID_COLUMNS; ++x)
    {
        const int pixelX = BOARD_X + x * CELL_SIZE;
        DrawLine(pixelX, BOARD_Y, pixelX, BOARD_Y + BOARD_HEIGHT, Fade(BLACK, 0.14f));
    }

    for (int y = 0; y <= GRID_ROWS; ++y)
    {
        const int pixelY = BOARD_Y + y * CELL_SIZE;
        DrawLine(BOARD_X, pixelY, BOARD_X + BOARD_WIDTH, pixelY, Fade(BLACK, 0.14f));
    }
}

static void DrawInfoLine(const char* label, const char* value, int y)
{
    /*
        Phase 1 的右侧信息栏只是静态占位数据。
        这里保留“标签 + 数值”的行结构，是为了后续接入真实状态时只替换数值部分，
        不需要重新整理信息栏布局。
    */
    DrawText(label, PANEL_X + 22, y, 20, kMutedTextColor);
    DrawText(value, PANEL_X + 150, y, 20, kTextColor);
}

static void DrawControlsText(int startY)
{
    DrawText("Controls", PANEL_X + 22, startY, 22, kAccentGreen);
    DrawText("Enter  Start", PANEL_X + 22, startY + 38, 18, kTextColor);
    DrawText("WASD / Arrows  Move", PANEL_X + 22, startY + 68, 18, kTextColor);
    DrawText("P  Pause", PANEL_X + 22, startY + 98, 18, kTextColor);
    DrawText("R  Restart", PANEL_X + 22, startY + 128, 18, kTextColor);
    DrawText("Esc  Quit", PANEL_X + 22, startY + 158, 18, kTextColor);
}

static void DrawInfoPanel(void)
{
    DrawRectangleRounded(
        (Rectangle){
            (float)PANEL_X,
            (float)PANEL_Y,
            (float)INFO_PANEL_WIDTH,
            (float)BOARD_HEIGHT
        },
        0.04f,
        8,
        kPanelBackground);

    DrawRectangleRoundedLines(
        (Rectangle){
            (float)PANEL_X,
            (float)PANEL_Y,
            (float)INFO_PANEL_WIDTH,
            (float)BOARD_HEIGHT
        },
        0.04f,
        8,
        kPanelStroke);

    DrawText("SNAKE", PANEL_X + 22, PANEL_Y + 24, 34, kTitleColor);
    DrawText("Phase 1 Layout", PANEL_X + 24, PANEL_Y + 64, 18, kMutedTextColor);

    DrawCircle(PANEL_X + INFO_PANEL_WIDTH - 48, PANEL_Y + 45, 13.0f, kAppleRed);
    DrawEllipse(PANEL_X + INFO_PANEL_WIDTH - 38, PANEL_Y + 33, 6.0f, 3.0f, kAccentGreen);

    DrawLine(PANEL_X + 22, PANEL_Y + 104, PANEL_X + INFO_PANEL_WIDTH - 22, PANEL_Y + 104, kPanelStroke);

    DrawInfoLine("Score", "0", PANEL_Y + 132);
    DrawInfoLine("Level", "1", PANEL_Y + 168);
    DrawInfoLine("Speed", "180 ms", PANEL_Y + 204);
    DrawInfoLine("Foods", "5 / 12", PANEL_Y + 240);
    DrawInfoLine("Best", "0", PANEL_Y + 276);

    DrawLine(PANEL_X + 22, PANEL_Y + 324, PANEL_X + INFO_PANEL_WIDTH - 22, PANEL_Y + 324, kPanelStroke);
    DrawControlsText(PANEL_Y + 350);
}

int main(void)
{
    /*
        raylib 相关调用目前只负责界面和窗口。
        后续新增的 game_core.c 只提供普通 C 数据，例如蛇身位置、分数和游戏状态；
        main.c 再把这些数据画到屏幕上。这样的拆分可以保证游戏核心不依赖 raylib，
        之后才能用 gtest 单独测试。
    */
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SnakeGame - Phase 1 Layout");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(kWindowBackground);

        DrawBoardBackground();
        DrawInfoPanel();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}