#include "game_core.h"

#include "raylib.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>



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
static const Color kSnakeHeadColor = { 110, 236, 132, 255 };
static const Color kSnakeBodyColor = { 57, 176, 91, 255 };
static const Color kSnakeBodyDarkColor = { 38, 128, 74, 255 };
static const Color kSnakeEyeColor = { 8, 18, 14, 255 };
static const Color kSnakeNostrilColor = { 7, 24, 15, 255 };
static const Color kSnakeMouthColor = { 6, 35, 18, 255 };
static const Color kSnakeBodyHighlightColor = { 136, 244, 154, 255 };
static const Color kSnakeRedHeadColor = { 246, 92, 91, 255 };
static const Color kSnakeRedBodyColor = { 210, 54, 64, 255 };
static const Color kSnakeRedBodyDarkColor = { 139, 34, 46, 255 };
static const Color kSnakeRedBodyHighlightColor = { 255, 150, 140, 255 };

#define SNAKE_RENDER_MAX_SEGMENTS (GRID_COLUMNS * GRID_ROWS)

typedef struct SnakeSegmentRenderData
{
    SnakeGridPosition position;
    bool isHead;
} SnakeSegmentRenderData;

typedef struct SnakeRenderData
{
    SnakeSegmentRenderData segments[SNAKE_RENDER_MAX_SEGMENTS];
    int segmentCount;
    SnakeDirection direction;
    SnakeColorMode colorMode;
} SnakeRenderData;


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


static bool BuildSnakeRenderData(const SnakeGameCore* game, SnakeRenderData* renderData)
{
    if (game == NULL || renderData == NULL)
    {
        return false;
    }


    const int snakeLength = SnakeGameGetSnakeLength(game);


    if (snakeLength < 0 || snakeLength > SNAKE_RENDER_MAX_SEGMENTS)
    {
        return false;
    }


    renderData->segmentCount = 0;
    renderData->direction = SnakeGameGetDirection(game);
    renderData->colorMode = SnakeGameGetSnakeColorMode(game);


    for (int i = 0; i < snakeLength; ++i)
    {
        SnakeGridPosition position;


        if (!SnakeGameGetSnakeSegment(game, i, &position))
        {
            return false;
        }


        renderData->segments[i].position = position;
        renderData->segments[i].isHead = (i == 0);
        renderData->segmentCount++;
    }

    return true;
}

static void HandleDirectionInput(SnakeGameCore* game)
{

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_UP);
    }

    else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_RIGHT);
    }

    else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_DOWN);
    }

    else if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_LEFT);
    }
}

static const char* GetStatusText(SnakeGameStatus status)
{
    switch (status)
    {
    case SNAKE_GAME_STATUS_READY:
        return "Ready";
    case SNAKE_GAME_STATUS_RUNNING:
        return "Running";
    case SNAKE_GAME_STATUS_PAUSED:
        return "Paused";
    case SNAKE_GAME_STATUS_DEAD:
        return "Game Over";
    default:
        return "Unknown";
    }
}

static void HandleGameInput(SnakeGameCore* game, float* moveTimer)
{
    if (game == NULL || moveTimer == NULL)
    {
        return;
    }

    /*
        界面层只负责把键盘输入翻译成“游戏命令”。

        例如 Enter 表示开始或重开，P 表示暂停/继续，方向键表示请求转向。
        真正决定 READY、RUNNING、PAUSED、DEAD 各自能不能移动蛇身的，是核心层。
        这样做可以保证：无论是窗口里操作，还是单元测试直接调用核心接口，状态规则都一致。
    */
    const SnakeGameStatus status = SnakeGameGetStatus(game);

    if (IsKeyPressed(KEY_ENTER))
    {
        if (status == SNAKE_GAME_STATUS_READY)
        {
            SnakeGameStart(game);
            *moveTimer = 0.0f;
        }
        else if (status == SNAKE_GAME_STATUS_DEAD)
        {
            SnakeGameRestart(game);
            *moveTimer = 0.0f;
        }
    }

    if (IsKeyPressed(KEY_P))
    {
        SnakeGameTogglePause(game);
        *moveTimer = 0.0f;
    }

    if (IsKeyPressed(KEY_R))
    {
        const SnakeGameStatus currentStatus = SnakeGameGetStatus(game);
        if (currentStatus == SNAKE_GAME_STATUS_RUNNING || currentStatus == SNAKE_GAME_STATUS_PAUSED)
        {
            SnakeGameRestart(game);
            *moveTimer = 0.0f;
        }
    }

    /*
        方向输入只在 RUNNING 状态交给核心层。

        如果在开始界面或暂停界面偷偷缓存方向，玩家恢复游戏时蛇可能突然转向。
        这里选择更直观的规则：只有蛇正在移动时，方向键才真正改变下一步方向。
    */
    if (SnakeGameGetStatus(game) == SNAKE_GAME_STATUS_RUNNING)
    {
        HandleDirectionInput(game);
    }
}
static void DrawBoardBackground(void)
{


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

static Color BlendSnakeSegmentColor(float amount, SnakeColorMode colorMode)
{

    const Color baseColor = (colorMode == SNAKE_COLOR_MODE_RED) ? kSnakeRedBodyColor : kSnakeBodyColor;
    const Color darkColor = (colorMode == SNAKE_COLOR_MODE_RED) ? kSnakeRedBodyDarkColor : kSnakeBodyDarkColor;

    Color color = {
        (unsigned char)(baseColor.r + (darkColor.r - baseColor.r) * amount),
        (unsigned char)(baseColor.g + (darkColor.g - baseColor.g) * amount),
        (unsigned char)(baseColor.b + (darkColor.b - baseColor.b) * amount),
        255
    };

    return color;
}


static void DrawSnakeBodySegment(SnakeGridPosition position, int segmentIndex, SnakeColorMode colorMode)
{

    Rectangle segmentRect = GridCellToRectangle(position.x, position.y);


    segmentRect.x += 3.0f;
    segmentRect.y += 3.0f;
    segmentRect.width -= 6.0f;
    segmentRect.height -= 6.0f;


    const float fadeStep = (float)(segmentIndex % 4) * 0.08f;
    const Color segmentColor = BlendSnakeSegmentColor(fadeStep, colorMode);


    DrawRectangleRounded(segmentRect, 0.26f, 8, segmentColor);


    Vector2 highlightStart = { segmentRect.x + 6.0f, segmentRect.y + 6.0f };
    Vector2 highlightEnd = { segmentRect.x + segmentRect.width * 0.56f, segmentRect.y + 6.0f };
    const Color highlightColor = (colorMode == SNAKE_COLOR_MODE_RED) ? kSnakeRedBodyHighlightColor : kSnakeBodyHighlightColor;
    DrawLineEx(highlightStart, highlightEnd, 2.0f, Fade(highlightColor, 0.28f));


    DrawRectangleRoundedLines(segmentRect, 0.26f, 8, Fade(kTitleColor, 0.16f));
}


static void DrawSnakeHead(SnakeGridPosition position, SnakeDirection direction, SnakeColorMode colorMode)
{

    Rectangle headRect = GridCellToRectangle(position.x, position.y);
    headRect.x += 2.0f;
    headRect.y += 2.0f;
    headRect.width -= 4.0f;
    headRect.height -= 4.0f;


    const Color headColor = (colorMode == SNAKE_COLOR_MODE_RED) ? kSnakeRedHeadColor : kSnakeHeadColor;
    DrawRectangleRounded(headRect, 0.30f, 10, headColor);
    DrawRectangleRoundedLines(headRect, 0.30f, 10, Fade(kTitleColor, 0.24f));


    Vector2 firstEye = { headRect.x + headRect.width * 0.66f, headRect.y + headRect.height * 0.32f };
    Vector2 secondEye = { headRect.x + headRect.width * 0.66f, headRect.y + headRect.height * 0.68f };
    Vector2 firstNostril = { headRect.x + headRect.width * 0.82f, headRect.y + headRect.height * 0.43f };
    Vector2 secondNostril = { headRect.x + headRect.width * 0.82f, headRect.y + headRect.height * 0.57f };
    Vector2 mouthStart = { headRect.x + headRect.width * 0.69f, headRect.y + headRect.height * 0.50f };
    Vector2 mouthEnd = { headRect.x + headRect.width * 0.78f, headRect.y + headRect.height * 0.50f };


    switch (direction)
    {
    case SNAKE_DIRECTION_UP:
        firstEye = (Vector2){ headRect.x + headRect.width * 0.32f, headRect.y + headRect.height * 0.34f };
        secondEye = (Vector2){ headRect.x + headRect.width * 0.68f, headRect.y + headRect.height * 0.34f };
        firstNostril = (Vector2){ headRect.x + headRect.width * 0.43f, headRect.y + headRect.height * 0.18f };
        secondNostril = (Vector2){ headRect.x + headRect.width * 0.57f, headRect.y + headRect.height * 0.18f };
        mouthStart = (Vector2){ headRect.x + headRect.width * 0.50f, headRect.y + headRect.height * 0.23f };
        mouthEnd = (Vector2){ headRect.x + headRect.width * 0.50f, headRect.y + headRect.height * 0.32f };
        break;
    case SNAKE_DIRECTION_DOWN:
        firstEye = (Vector2){ headRect.x + headRect.width * 0.32f, headRect.y + headRect.height * 0.66f };
        secondEye = (Vector2){ headRect.x + headRect.width * 0.68f, headRect.y + headRect.height * 0.66f };
        firstNostril = (Vector2){ headRect.x + headRect.width * 0.43f, headRect.y + headRect.height * 0.82f };
        secondNostril = (Vector2){ headRect.x + headRect.width * 0.57f, headRect.y + headRect.height * 0.82f };
        mouthStart = (Vector2){ headRect.x + headRect.width * 0.50f, headRect.y + headRect.height * 0.68f };
        mouthEnd = (Vector2){ headRect.x + headRect.width * 0.50f, headRect.y + headRect.height * 0.77f };
        break;
    case SNAKE_DIRECTION_LEFT:
        firstEye = (Vector2){ headRect.x + headRect.width * 0.34f, headRect.y + headRect.height * 0.32f };
        secondEye = (Vector2){ headRect.x + headRect.width * 0.34f, headRect.y + headRect.height * 0.68f };
        firstNostril = (Vector2){ headRect.x + headRect.width * 0.18f, headRect.y + headRect.height * 0.43f };
        secondNostril = (Vector2){ headRect.x + headRect.width * 0.18f, headRect.y + headRect.height * 0.57f };
        mouthStart = (Vector2){ headRect.x + headRect.width * 0.31f, headRect.y + headRect.height * 0.50f };
        mouthEnd = (Vector2){ headRect.x + headRect.width * 0.22f, headRect.y + headRect.height * 0.50f };
        break;
    case SNAKE_DIRECTION_RIGHT:
    default:
        break;
    }


    DrawCircleV(firstEye, 2.6f, kSnakeEyeColor);
    DrawCircleV(secondEye, 2.6f, kSnakeEyeColor);
    DrawCircleV(firstNostril, 1.25f, kSnakeNostrilColor);
    DrawCircleV(secondNostril, 1.25f, kSnakeNostrilColor);
    DrawLineEx(mouthStart, mouthEnd, 1.6f, kSnakeMouthColor);
}

static void DrawSnake(const SnakeRenderData* renderData)
{
    if (renderData == NULL)
    {
        return;
    }


    for (int i = renderData->segmentCount - 1; i >= 0; --i)
    {
        const SnakeSegmentRenderData* segment = &renderData->segments[i];


        if (segment->isHead)
        {
            DrawSnakeHead(segment->position, renderData->direction, renderData->colorMode);
        }
        else
        {
            DrawSnakeBodySegment(segment->position, i, renderData->colorMode);
        }
    }
}

static void DrawApple(SnakeGridPosition position)
{
    /*
        食物在核心层同样只保存为网格坐标。

        核心层只需要知道“第几列、第几行有苹果”，不需要知道苹果在屏幕上画多大。
        main.c 在这里把网格坐标换算成像素圆形和叶子，保持“规则”和“表现”分离。
    */
    const Rectangle cell = GridCellToRectangle(position.x, position.y);
    const Vector2 appleCenter = {
        cell.x + cell.width * 0.50f,
        cell.y + cell.height * 0.56f
    };

    DrawCircleV(appleCenter, cell.width * 0.30f, kAppleRed);
    DrawCircle((int)(appleCenter.x - cell.width * 0.10f), (int)(appleCenter.y - cell.height * 0.06f), cell.width * 0.14f, Fade(WHITE, 0.18f));
    DrawLineEx(
        (Vector2){ appleCenter.x + 1.0f, appleCenter.y - cell.height * 0.28f },
        (Vector2){ appleCenter.x + 4.0f, appleCenter.y - cell.height * 0.44f },
        2.0f,
        kSnakeMouthColor);
    DrawEllipse((int)(appleCenter.x + 8.0f), (int)(appleCenter.y - 10.0f), 6.0f, 3.2f, kAccentGreen);
}

static void DrawFoods(const SnakeGameCore* game)
{
    if (game == NULL)
    {
        return;
    }

    for (int i = 0; i < SnakeGameGetFoodCount(game); ++i)
    {
        SnakeGridPosition food;
        if (SnakeGameGetFoodPosition(game, i, &food))
        {
            DrawApple(food);
        }
    }
}

static void DrawCenteredBoardText(const char* text, int y, int fontSize, Color color)
{
    const int textWidth = MeasureText(text, fontSize);
    DrawText(text, BOARD_X + (BOARD_WIDTH - textWidth) / 2, y, fontSize, color);
}

static void DrawBoardOverlayBackground(void)
{
    /*
        开始、暂停、结束界面使用覆盖层，而不是清空棋盘重新画一个页面。

        这样玩家仍然能看到当前真实的蛇身链表和食物位置：
        - READY 能看到初始蛇和初始苹果；
        - PAUSED 能看到暂停瞬间的蛇形；
        - DEAD 能看到死亡时停住的位置。
    */
    DrawRectangle(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, Fade(BLACK, 0.64f));
    DrawRectangleLines(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, Fade(kAccentGreen, 0.36f));
}

static void DrawStartOverlay(void)
{
    DrawBoardOverlayBackground();
    DrawCenteredBoardText("SNAKE", BOARD_Y + 178, 54, kTitleColor);
    DrawCenteredBoardText("Press Enter to start", BOARD_Y + 252, 24, kAccentGreen);
    DrawCenteredBoardText("WASD / Arrow Keys to move", BOARD_Y + 302, 20, kTextColor);
    DrawCenteredBoardText("P pauses, R restarts, Esc quits", BOARD_Y + 334, 20, kMutedTextColor);
}

static void DrawPauseOverlay(void)
{
    DrawBoardOverlayBackground();
    DrawCenteredBoardText("PAUSED", BOARD_Y + 210, 46, kTitleColor);
    DrawCenteredBoardText("Press P to resume", BOARD_Y + 282, 24, kAccentGreen);
    DrawCenteredBoardText("Press R to restart", BOARD_Y + 322, 20, kTextColor);
}

static void DrawGameOverOverlay(const SnakeGameCore* game)
{
    char scoreText[64];
    char levelText[64];
    char foodsText[64];

    snprintf(scoreText, sizeof(scoreText), "Score: %d", SnakeGameGetScore(game));
    snprintf(levelText, sizeof(levelText), "Level: %d", SnakeGameGetLevel(game));
    snprintf(foodsText, sizeof(foodsText), "Foods Eaten: %d", SnakeGameGetFoodsEaten(game));

    DrawBoardOverlayBackground();
    DrawCenteredBoardText("GAME OVER", BOARD_Y + 150, 46, kAppleRed);
    DrawCenteredBoardText(scoreText, BOARD_Y + 226, 24, kTitleColor);
    DrawCenteredBoardText(levelText, BOARD_Y + 262, 22, kTextColor);
    DrawCenteredBoardText(foodsText, BOARD_Y + 296, 22, kTextColor);
    DrawCenteredBoardText("Best: 0", BOARD_Y + 330, 22, kMutedTextColor);
    DrawCenteredBoardText("Press Enter to play again", BOARD_Y + 388, 24, kAccentGreen);
}

static void DrawStateOverlay(const SnakeGameCore* game)
{
    if (game == NULL)
    {
        return;
    }

    switch (SnakeGameGetStatus(game))
    {
    case SNAKE_GAME_STATUS_READY:
        DrawStartOverlay();
        break;
    case SNAKE_GAME_STATUS_PAUSED:
        DrawPauseOverlay();
        break;
    case SNAKE_GAME_STATUS_DEAD:
        DrawGameOverOverlay(game);
        break;
    case SNAKE_GAME_STATUS_RUNNING:
    default:
        break;
    }
}
static void DrawInfoLine(const char* label, const char* value, int y)
{

    DrawText(label, PANEL_X + 22, y, 20, kMutedTextColor);
    DrawText(value, PANEL_X + 150, y, 20, kTextColor);
}

static void DrawControlsText(int startY)
{
    DrawText("Controls", PANEL_X + 22, startY, 22, kAccentGreen);
    DrawText("Enter  Start / Retry", PANEL_X + 22, startY + 38, 18, kTextColor);
    DrawText("WASD / Arrows  Move", PANEL_X + 22, startY + 68, 18, kTextColor);
    DrawText("P  Pause / Resume", PANEL_X + 22, startY + 98, 18, kTextColor);
    DrawText("R  Restart", PANEL_X + 22, startY + 128, 18, kTextColor);
    DrawText("Esc  Quit", PANEL_X + 22, startY + 158, 18, kTextColor);
}

static void DrawInfoPanel(const SnakeGameCore* game)
{
    char scoreText[32];
    char levelText[32];
    char speedText[32];
    char foodsText[32];

    snprintf(scoreText, sizeof(scoreText), "%d", SnakeGameGetScore(game));
    snprintf(levelText, sizeof(levelText), "%d", SnakeGameGetLevel(game));
    snprintf(speedText, sizeof(speedText), "%d ms", SnakeGameGetMoveIntervalMs(game));
    snprintf(foodsText, sizeof(foodsText), "%d / %d", SnakeGameGetFoodCount(game), SNAKE_GAME_MAX_FOOD_COUNT);

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
    DrawText("Phase 6 States", PANEL_X + 24, PANEL_Y + 64, 18, kMutedTextColor);

    DrawCircle(PANEL_X + INFO_PANEL_WIDTH - 48, PANEL_Y + 45, 13.0f, kAppleRed);
    DrawEllipse(PANEL_X + INFO_PANEL_WIDTH - 38, PANEL_Y + 33, 6.0f, 3.0f, kAccentGreen);

    DrawLine(PANEL_X + 22, PANEL_Y + 104, PANEL_X + INFO_PANEL_WIDTH - 22, PANEL_Y + 104, kPanelStroke);

    /*
        右侧信息栏只通过核心层 getter 读取状态。

        分数、等级、速度、食物数量都不在界面层重新维护一份副本。
        这样核心层一旦因为吃食物或升级改变数据，信息栏下一帧就会显示真实结果。
    */
    DrawInfoLine("Score", scoreText, PANEL_Y + 132);
    DrawInfoLine("Level", levelText, PANEL_Y + 168);
    DrawInfoLine("Speed", speedText, PANEL_Y + 204);
    DrawInfoLine("Foods", foodsText, PANEL_Y + 240);
    DrawInfoLine("Best", "0", PANEL_Y + 276);
    DrawInfoLine("State", GetStatusText(SnakeGameGetStatus(game)), PANEL_Y + 312);

    DrawLine(PANEL_X + 22, PANEL_Y + 356, PANEL_X + INFO_PANEL_WIDTH - 22, PANEL_Y + 356, kPanelStroke);
    DrawControlsText(PANEL_Y + 382);
}
int main(void)
{
    /*
        主循环遵守“界面层”和“核心层”分离：

        1. raylib 负责窗口、键盘和绘制；
        2. game_core.c 负责玩法规则、状态机和蛇身链表；
        3. main.c 每帧先把输入转成核心命令，再按核心状态决定是否推进，最后绘制结果。
    */
    SetConfigFlags(FLAG_VSYNC_HINT);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SnakeGame - Phase 6 States");
    SetTargetFPS(60);

    SnakeGameCore game;

    if (!SnakeGameInit(&game))
    {
        CloseWindow();
        return 1;
    }

    /*
        moveTimer 把“每秒绘制 60 次”的窗口循环，转换成“每隔若干毫秒移动一格”的游戏节奏。

        移动间隔从核心层读取，而不是在界面层写死。这样升级后速度变快时，
        main.c 不需要知道速度曲线，只要相信 SnakeGameGetMoveIntervalMs 的结果。
    */
    float moveTimer = 0.0f;

    while (!WindowShouldClose())
    {
        HandleGameInput(&game, &moveTimer);

        if (SnakeGameGetStatus(&game) == SNAKE_GAME_STATUS_RUNNING)
        {
            const float moveIntervalSeconds = (float)SnakeGameGetMoveIntervalMs(&game) / 1000.0f;

            moveTimer += GetFrameTime();
            if (moveTimer >= moveIntervalSeconds)
            {
                SnakeGameStep(&game);
                moveTimer = 0.0f;
            }
        }
        else
        {
            /*
                非 RUNNING 状态会同时冻结计时器和蛇身链表。

                如果暂停期间继续累计时间，玩家恢复游戏时可能立刻移动一格；
                这里清零计时器，让恢复后的下一步从完整的移动间隔重新开始。
            */
            moveTimer = 0.0f;
        }

        BeginDrawing();
        ClearBackground(kWindowBackground);

        DrawBoardBackground();
        DrawFoods(&game);

        SnakeRenderData snakeRenderData;
        if (BuildSnakeRenderData(&game, &snakeRenderData))
        {
            DrawSnake(&snakeRenderData);
        }

        DrawStateOverlay(&game);
        DrawInfoPanel(&game);

        EndDrawing();
    }

    SnakeGameDestroy(&game);
    CloseWindow();
    return 0;
}
