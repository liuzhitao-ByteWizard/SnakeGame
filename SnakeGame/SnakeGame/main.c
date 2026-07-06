#include "game_core.h"
/* 引入最高分存档模块，让界面层可以读取和保存本地最高分。 */
#include "high_score.h"

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

/* 定义最高分文本文件名，文件会放在程序运行时的工作目录中。 */
#define HIGH_SCORE_FILE_PATH "highscore.txt"

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

static void UpdateHighScoreAfterDeath(const SnakeGameCore* game, int* highScore)
{
    /* 先检查游戏对象和最高分指针是否有效，避免后面访问空指针。 */
    if (game == NULL || highScore == NULL)
    {
        /* 参数无效时直接返回，最高分保持原样。 */
        return;
    }

    /*
        最高分属于“界面层 + 存档模块”的协作数据，不属于 game_core 的核心规则。
        核心层只负责告诉我们本局分数是多少；界面层在死亡那一帧比较分数，
        如果刷新纪录，就调用存档模块写入本地文本文件。
    */

    /* 判断本局分数是否超过当前内存中的最高分。 */
    if (SnakeGameGetScore(game) > *highScore)
    {
        /* 先更新内存中的最高分，让结束界面立刻显示新纪录。 */
        *highScore = SnakeGameGetScore(game);

        /* 再尝试写入文本文件；即使写入失败，也不打断当前游戏流程。 */
        SnakeHighScoreSave(HIGH_SCORE_FILE_PATH, *highScore);
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

static void DrawStartOverlay(int highScore)
{
    /* 准备开始界面上显示的最高分文本。 */
    char bestText[64];

    /* 把最高分整数格式化成可绘制的字符串。 */
    snprintf(bestText, sizeof(bestText), "Best: %d", highScore);

    /* 先绘制覆盖层背景，让开始提示从棋盘上清晰浮出来。 */
    DrawBoardOverlayBackground();

    /* 绘制游戏标题。 */
    DrawCenteredBoardText("SNAKE", BOARD_Y + 178, 54, kTitleColor);

    /* 绘制开始操作提示。 */
    DrawCenteredBoardText("Press Enter to start", BOARD_Y + 252, 24, kAccentGreen);

    /* 绘制从本地文件读取到的最高分。 */
    DrawCenteredBoardText(bestText, BOARD_Y + 294, 22, kTextColor);

    /* 绘制移动按键提示。 */
    DrawCenteredBoardText("WASD / Arrow Keys to move", BOARD_Y + 336, 20, kTextColor);

    /* 绘制暂停、重开和退出提示。 */
    DrawCenteredBoardText("P pauses, R restarts, Esc quits", BOARD_Y + 368, 20, kMutedTextColor);
}

static void DrawPauseOverlay(void)
{
    DrawBoardOverlayBackground();
    DrawCenteredBoardText("PAUSED", BOARD_Y + 210, 46, kTitleColor);
    DrawCenteredBoardText("Press P to resume", BOARD_Y + 282, 24, kAccentGreen);
    DrawCenteredBoardText("Press R to restart", BOARD_Y + 322, 20, kTextColor);
}

static void DrawGameOverOverlay(const SnakeGameCore* game, int highScore)
{
    /* 准备结束界面上显示的本局分数字符串。 */
    char scoreText[64];

    /* 准备结束界面上显示的等级字符串。 */
    char levelText[64];

    /* 准备结束界面上显示的本局吃到食物数量字符串。 */
    char foodsText[64];

    /* 准备结束界面上显示的最高分字符串。 */
    char bestText[64];

    /* 把本局分数格式化成可绘制字符串。 */
    snprintf(scoreText, sizeof(scoreText), "Score: %d", SnakeGameGetScore(game));

    /* 把本局等级格式化成可绘制字符串。 */
    snprintf(levelText, sizeof(levelText), "Level: %d", SnakeGameGetLevel(game));

    /* 把本局吃到食物数量格式化成可绘制字符串。 */
    snprintf(foodsText, sizeof(foodsText), "Foods Eaten: %d", SnakeGameGetFoodsEaten(game));

    /* 把当前最高分格式化成可绘制字符串。 */
    snprintf(bestText, sizeof(bestText), "Best: %d", highScore);

    /* 先绘制结束覆盖层背景，让结果信息更醒目。 */
    DrawBoardOverlayBackground();

    /* 绘制游戏结束标题。 */
    DrawCenteredBoardText("GAME OVER", BOARD_Y + 150, 46, kAppleRed);

    /* 绘制本局分数。 */
    DrawCenteredBoardText(scoreText, BOARD_Y + 226, 24, kTitleColor);

    /* 绘制本局等级。 */
    DrawCenteredBoardText(levelText, BOARD_Y + 262, 22, kTextColor);

    /* 绘制本局吃到的食物数量。 */
    DrawCenteredBoardText(foodsText, BOARD_Y + 296, 22, kTextColor);

    /* 绘制当前最高分，若本局刷新纪录，这里会立即显示新值。 */
    DrawCenteredBoardText(bestText, BOARD_Y + 330, 22, kMutedTextColor);

    /* 绘制再来一局提示。 */
    DrawCenteredBoardText("Press Enter to play again", BOARD_Y + 388, 24, kAccentGreen);
}

static void DrawStateOverlay(const SnakeGameCore* game, int highScore)
{
    /* 先检查游戏对象是否有效，避免读取空指针。 */
    if (game == NULL)
    {
        /* 游戏对象无效时不绘制任何状态覆盖层。 */
        return;
    }

    /* 根据核心层当前状态，选择要绘制的覆盖层。 */
    switch (SnakeGameGetStatus(game))
    {
    case SNAKE_GAME_STATUS_READY:
        /* 准备状态绘制开始界面，并显示已读取的最高分。 */
        DrawStartOverlay(highScore);
        break;
    case SNAKE_GAME_STATUS_PAUSED:
        /* 暂停状态绘制暂停界面。 */
        DrawPauseOverlay();
        break;
    case SNAKE_GAME_STATUS_DEAD:
        /* 死亡状态绘制结束界面，并显示当前最高分。 */
        DrawGameOverOverlay(game, highScore);
        break;
    case SNAKE_GAME_STATUS_RUNNING:
    default:
        /* 运行状态不绘制覆盖层，让玩家完整看到棋盘。 */
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

static void DrawInfoPanel(const SnakeGameCore* game, int highScore)
{
    /* 准备右侧信息栏显示的分数字符串。 */
    char scoreText[32];

    /* 准备右侧信息栏显示的等级字符串。 */
    char levelText[32];

    /* 准备右侧信息栏显示的速度字符串。 */
    char speedText[32];

    /* 准备右侧信息栏显示的食物数量字符串。 */
    char foodsText[32];

    /* 准备右侧信息栏显示的最高分字符串。 */
    char bestText[32];

    /* 把当前分数格式化成信息栏文本。 */
    snprintf(scoreText, sizeof(scoreText), "%d", SnakeGameGetScore(game));

    /* 把当前等级格式化成信息栏文本。 */
    snprintf(levelText, sizeof(levelText), "%d", SnakeGameGetLevel(game));

    /* 把当前移动间隔格式化成信息栏文本。 */
    snprintf(speedText, sizeof(speedText), "%d ms", SnakeGameGetMoveIntervalMs(game));

    /* 把当前食物数量格式化成信息栏文本。 */
    snprintf(foodsText, sizeof(foodsText), "%d / %d", SnakeGameGetFoodCount(game), SNAKE_GAME_MAX_FOOD_COUNT);

    /* 把本地读取或本局刷新后的最高分格式化成信息栏文本。 */
    snprintf(bestText, sizeof(bestText), "%d", highScore);

    /* 绘制右侧信息栏背景。 */
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

    /* 绘制右侧信息栏边框。 */
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

    /* 绘制信息栏标题。 */
    DrawText("SNAKE", PANEL_X + 22, PANEL_Y + 24, 34, kTitleColor);

    /* 绘制当前阶段说明。 */
    DrawText("Phase 7 Save", PANEL_X + 24, PANEL_Y + 64, 18, kMutedTextColor);

    /* 绘制右上角苹果装饰的红色果身。 */
    DrawCircle(PANEL_X + INFO_PANEL_WIDTH - 48, PANEL_Y + 45, 13.0f, kAppleRed);

    /* 绘制右上角苹果装饰的叶子。 */
    DrawEllipse(PANEL_X + INFO_PANEL_WIDTH - 38, PANEL_Y + 33, 6.0f, 3.0f, kAccentGreen);

    /* 绘制标题和数据区之间的分隔线。 */
    DrawLine(PANEL_X + 22, PANEL_Y + 104, PANEL_X + INFO_PANEL_WIDTH - 22, PANEL_Y + 104, kPanelStroke);

    /* 绘制当前分数。 */
    DrawInfoLine("Score", scoreText, PANEL_Y + 132);

    /* 绘制当前等级。 */
    DrawInfoLine("Level", levelText, PANEL_Y + 168);

    /* 绘制当前速度。 */
    DrawInfoLine("Speed", speedText, PANEL_Y + 204);

    /* 绘制当前食物数量。 */
    DrawInfoLine("Foods", foodsText, PANEL_Y + 240);

    /* 绘制最高分。 */
    DrawInfoLine("Best", bestText, PANEL_Y + 276);

    /* 绘制当前游戏状态。 */
    DrawInfoLine("State", GetStatusText(SnakeGameGetStatus(game)), PANEL_Y + 312);

    /* 绘制数据区和操作提示区之间的分隔线。 */
    DrawLine(PANEL_X + 22, PANEL_Y + 356, PANEL_X + INFO_PANEL_WIDTH - 22, PANEL_Y + 356, kPanelStroke);

    /* 绘制操作提示列表。 */
    DrawControlsText(PANEL_Y + 382);
}
int main(void)
{
    /*
        主循环把“界面层”和“核心层”连接起来：
        1. raylib 负责窗口、键盘和绘制；
        2. game_core.c 负责玩法规则；
        3. high_score.c 负责把最高分保存到本地文本文件。
    */

    /* 开启垂直同步，让画面刷新更稳定。 */
    SetConfigFlags(FLAG_VSYNC_HINT);

    /* 创建固定尺寸窗口，并在标题中标明当前 Phase 7 已接入本地存档。 */
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SnakeGame - Phase 7 Save");

    /* 设置目标帧率为 60 帧，移动节奏仍由核心层的毫秒间隔控制。 */
    SetTargetFPS(60);

    /* 创建游戏核心状态对象，蛇身、食物、分数等规则数据都放在这里。 */
    SnakeGameCore game;

    /* 初始化核心状态，准备初始蛇身、初始食物和 READY 状态。 */
    if (!SnakeGameInit(&game))
    {
        /* 如果核心状态初始化失败，先关闭窗口资源。 */
        CloseWindow();

        /* 返回非 0 值，表示程序启动失败。 */
        return 1;
    }

    /*
        最高分只在程序启动时读取一次，然后保存在 main.c 的局部变量中。
        重开一局会重置 SnakeGameCore，但不会重置这个最高分变量。
    */

    /* 从工作目录下的 highscore.txt 读取最高分；读取失败或非法内容都会得到 0。 */
    int highScore = SnakeHighScoreLoad(HIGH_SCORE_FILE_PATH);

    /*
        moveTimer 把“每秒绘制 60 次”的窗口循环，转换成“每隔若干毫秒移动一格”的游戏节奏。
        移动间隔从核心层读取，因此等级提升后界面层不需要自己重新计算速度。
    */

    /* 初始化移动计时器，开始时不累积任何移动时间。 */
    float moveTimer = 0.0f;

    /* 只要窗口没有请求关闭，就持续运行游戏主循环。 */
    while (!WindowShouldClose())
    {
        /* 先处理键盘输入，把 Enter、P、R 和方向键转换成核心层命令。 */
        HandleGameInput(&game, &moveTimer);

        /* 只有 RUNNING 状态才推进蛇的移动。 */
        if (SnakeGameGetStatus(&game) == SNAKE_GAME_STATUS_RUNNING)
        {
            /* 把核心层给出的毫秒移动间隔换算成秒，方便和 GetFrameTime 相加比较。 */
            const float moveIntervalSeconds = (float)SnakeGameGetMoveIntervalMs(&game) / 1000.0f;

            /* 累加这一帧经过的时间。 */
            moveTimer += GetFrameTime();

            /* 当累计时间达到移动间隔时，才让核心层前进一步。 */
            if (moveTimer >= moveIntervalSeconds)
            {
                /* 记录推进前的状态，用来判断这一帧是否刚好从运行进入死亡。 */
                const SnakeGameStatus statusBeforeStep = SnakeGameGetStatus(&game);

                /* 推进游戏核心一步，内部会处理移动、吃食物、撞墙和自撞。 */
                SnakeGameStep(&game);

                /* 如果这一帧刚刚死亡，就检查是否需要刷新本地最高分。 */
                if (statusBeforeStep != SNAKE_GAME_STATUS_DEAD &&
                    SnakeGameGetStatus(&game) == SNAKE_GAME_STATUS_DEAD)
                {
                    /* 比较本局分数和最高分，必要时写入 highscore.txt。 */
                    UpdateHighScoreAfterDeath(&game, &highScore);
                }

                /* 本次移动已经完成，重置计时器等待下一次移动间隔。 */
                moveTimer = 0.0f;
            }
        }
        else
        {
            /*
                非 RUNNING 状态下不累积移动时间。
                这样暂停、开始界面或死亡界面停留再恢复时，不会突然连续移动。
            */

            /* 清空移动计时器，让下一次进入 RUNNING 时从 0 开始计时。 */
            moveTimer = 0.0f;
        }

        /* 开始一帧绘制。 */
        BeginDrawing();

        /* 清空窗口背景，避免上一帧画面残留。 */
        ClearBackground(kWindowBackground);

        /* 绘制棋盘背景。 */
        DrawBoardBackground();

        /* 绘制当前所有食物。 */
        DrawFoods(&game);

        /* 准备蛇身渲染数据，把核心层链表数据转换成绘制层容易使用的数组。 */
        SnakeRenderData snakeRenderData;

        /* 如果蛇身渲染数据构建成功，就绘制整条蛇。 */
        if (BuildSnakeRenderData(&game, &snakeRenderData))
        {
            /* 绘制蛇头和蛇身。 */
            DrawSnake(&snakeRenderData);
        }

        /* 根据 READY、PAUSED、DEAD 状态绘制覆盖层，并传入最高分用于显示。 */
        DrawStateOverlay(&game, highScore);

        /* 绘制右侧信息栏，并传入最高分用于显示。 */
        DrawInfoPanel(&game, highScore);

        /* 结束一帧绘制，把画面显示到窗口。 */
        EndDrawing();
    }

    /* 程序退出前销毁核心层链表，释放蛇身节点。 */
    SnakeGameDestroy(&game);

    /* 关闭 raylib 窗口。 */
    CloseWindow();

    /* 返回 0，表示程序正常结束。 */
    return 0;
}
