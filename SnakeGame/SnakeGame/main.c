#include "game_core.h"

#include "raylib.h"

#include <stdbool.h>
#include <stddef.h>

/*
    Phase 3 在静态棋盘上接入游戏核心的初始蛇身。

    game_core.c 仍然只保存网格坐标和链表结构，不关心窗口、颜色或像素。
    main.c 作为 raylib 界面层，先从核心层读取只读蛇身快照，再把每一节
    网格坐标转换成屏幕上的圆角方块。这样既能看到真实核心状态，也不会
    让界面层绕过核心接口直接修改双向循环链表。
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
#define MOVE_INTERVAL_SECONDS 0.18f

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

/*
    把一个“网格坐标”转换成 raylib 绘制时需要的“像素矩形”。

    项目中的游戏位置统一使用网格坐标，而不是屏幕像素：
    (0, 0) 表示左上角格子，x 向右增加，y 向下增加。
    只有界面层需要知道像素位置；游戏核心只应该关心蛇头在第几列第几行。
    做到这层拆分后，核心逻辑就不依赖 raylib，也不依赖窗口尺寸，后续更容易测试。
*/
static Rectangle GridCellToRectangle(int gridX, int gridY)
{
    /* 左上角像素 = 棋盘起点 + 网格下标 * 单元格像素大小。 */
    Rectangle cell = {
        (float)(BOARD_X + gridX * CELL_SIZE),
        (float)(BOARD_Y + gridY * CELL_SIZE),
        (float)CELL_SIZE,
        (float)CELL_SIZE
    };

    /* 返回的是绘制用矩形，核心层仍然只知道 gridX 和 gridY。 */
    return cell;
}

/*
    将游戏核心中的只读状态转换成界面层可以绘制的数据。

    游戏核心只保存“网格坐标”，因为规则判断关心的是蛇在第几列、第几行，
    而不是屏幕上的像素位置。raylib 界面层再把这些网格坐标转换成像素矩形，
    负责颜色、圆角、眼睛等视觉表现。这样 game_core.c 不需要包含 raylib，
    后续也能继续用 Google Test 单独测试核心规则。

    这里通过 SnakeGameGetSnakeSegment 逐节读取蛇身，而不是直接访问
    game->snakeBody。双向循环链表是核心层的内部存储方式，界面层只需要
    一个安全、有顺序的快照：下标 0 对应链表头部，也就是蛇头；后面的下标
    依次向蛇尾移动。
*/
static bool BuildSnakeRenderData(const SnakeGameCore* game, SnakeRenderData* renderData)
{
    if (game == NULL || renderData == NULL)
    {
        return false;
    }

    /* 先读取蛇长，用它决定本帧需要从核心层拷贝多少节蛇身。 */
    const int snakeLength = SnakeGameGetSnakeLength(game);

    /* 渲染缓冲区最大只能容纳整张地图的格子数，超出说明核心状态异常。 */
    if (snakeLength < 0 || snakeLength > SNAKE_RENDER_MAX_SEGMENTS)
    {
        return false;
    }

    /* 每帧重新生成渲染快照，所以先清空数量，再记录当前蛇头朝向。 */
    renderData->segmentCount = 0;
    renderData->direction = SnakeGameGetDirection(game);
    renderData->colorMode = SnakeGameGetSnakeColorMode(game);

    /* 按链表顺序逐节读取蛇身：i 为 0 是蛇头，后面依次靠近蛇尾。 */
    for (int i = 0; i < snakeLength; ++i)
    {
        SnakeGridPosition position;

        /* 通过核心层查询接口取坐标，界面层不直接碰 snakeBody 链表指针。 */
        if (!SnakeGameGetSnakeSegment(game, i, &position))
        {
            return false;
        }

        /* 保存这一节的网格坐标，并标记第 0 节为蛇头，方便后面选择绘制方式。 */
        renderData->segments[i].position = position;
        renderData->segments[i].isHead = (i == 0);
        renderData->segmentCount++;
    }

    return true;
}

static void HandleDirectionInput(SnakeGameCore* game)
{
    /* 界面层只负责把“上方向键或 W”翻译为“向上”，是否合法交给核心层判断。 */
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_UP);
    }
    /* 界面层只负责把“右方向键或 D”翻译为“向右”，不在这里处理禁止反向规则。 */
    else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_RIGHT);
    }
    /* 界面层只负责把“下方向键或 S”翻译为“向下”，核心层会统一处理游戏规则。 */
    else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_DOWN);
    }
    /* 界面层只负责把“左方向键或 A”翻译为“向左”，这样键盘和未来输入方式规则一致。 */
    else if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
    {
        SnakeGameRequestDirection(game, SNAKE_DIRECTION_LEFT);
    }
}

static void DrawBoardBackground(void)
{
    /* 
        棋盘使用 GRID_COLUMNS 和 GRID_ROWS 逐格绘制。
        后续游戏核心也会使用同样的 30 x 20 地图尺寸；Phase 1 先把棋盘画出来，
        可以直观看到界面是否已经符合规格。
    */
    /* 先画棋盘外框，让游戏区域和右侧信息栏在视觉上分开。 */
    DrawRectangle(
        BOARD_X - BOARD_BORDER,
        BOARD_Y - BOARD_BORDER,
        BOARD_WIDTH + BOARD_BORDER * 2,
        BOARD_HEIGHT + BOARD_BORDER * 2,
        kBoardBorderColor);

    /* 再逐行逐列画 30 x 20 个格子，和核心层地图尺寸保持一致。 */
    for (int y = 0; y < GRID_ROWS; ++y)
    {
        for (int x = 0; x < GRID_COLUMNS; ++x)
        {
            /* 用 x + y 的奇偶性做棋盘格交替颜色，帮助玩家分辨每个格子。 */
            const bool useGreenCell = ((x + y) % 2) == 0;
            const Color cellColor = useGreenCell ? kGridGreen : kGridDark;

            /* 把当前网格坐标转成屏幕矩形后，再交给 raylib 绘制。 */
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

static Color BlendSnakeSegmentColor(float amount, SnakeColorMode colorMode)
{
    /*
        身体颜色仍然保留“深浅渐变”的表现，只是根据核心层的颜色模式换一套调色板。
        绿色是开局默认颜色；吃到红苹果后，核心会把颜色模式设为 RED，界面层就用红色身体。
    */
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

/*
    在一个网格格子内部绘制一节蛇身。

    蛇身没有把格子完全填满，而是留出一点内边距，这样棋盘底色还能露出来，
    每一个链表节点也会更像一节独立身体。颜色按照节段下标做轻微深浅变化，
    读代码和看画面时都能更容易理解：从链表头部遍历到尾部，就是从蛇头走到
    蛇尾的顺序。

    这里额外在左上区域画一条很淡的高光。它不是游戏规则的一部分，只是界面层
    的视觉处理：在不改变“圆角方块蛇”的前提下，让蛇身看起来更清爽、有一点
    立体感，同时仍然保持每一节都清楚地落在自己的格子里。
*/
static void DrawSnakeBodySegment(SnakeGridPosition position, int segmentIndex, SnakeColorMode colorMode)
{
    /* 先把这一节蛇身的网格坐标转换成屏幕矩形。 */
    Rectangle segmentRect = GridCellToRectangle(position.x, position.y);

    /* 缩小一点矩形，给格子边缘留出空隙，让每节身体更容易分辨。 */
    segmentRect.x += 3.0f;
    segmentRect.y += 3.0f;
    segmentRect.width -= 6.0f;
    segmentRect.height -= 6.0f;

    /* 按节段下标循环取深浅变化，避免整条蛇看起来只有一块纯色。 */
    const float fadeStep = (float)(segmentIndex % 4) * 0.08f;
    const Color segmentColor = BlendSnakeSegmentColor(fadeStep, colorMode);

    /* 画圆角主体，对应链表中的一个身体节点。 */
    DrawRectangleRounded(segmentRect, 0.26f, 8, segmentColor);

    /* 在身体左上方加一条淡高光，提高立体感，但不影响核心逻辑。 */
    Vector2 highlightStart = { segmentRect.x + 6.0f, segmentRect.y + 6.0f };
    Vector2 highlightEnd = { segmentRect.x + segmentRect.width * 0.56f, segmentRect.y + 6.0f };
    const Color highlightColor = (colorMode == SNAKE_COLOR_MODE_RED) ? kSnakeRedBodyHighlightColor : kSnakeBodyHighlightColor;
    DrawLineEx(highlightStart, highlightEnd, 2.0f, Fade(highlightColor, 0.28f));

    /* 最后画一圈很淡的描边，让绿色蛇身从深色棋盘上更清楚地分离出来。 */
    DrawRectangleRoundedLines(segmentRect, 0.26f, 8, Fade(kTitleColor, 0.16f));
}

/*
    绘制核心快照中的第一节，也就是蛇头。

    蛇头和身体使用同一套“网格坐标转像素矩形”的方式，但颜色更亮，并额外画出
    眼睛、鼻孔和嘴巴。眼睛、鼻孔、嘴巴都根据核心层当前方向计算；Phase 3 只有
    初始向右，但这里先把上、下、左、右都处理好，后续加入移动和转向时不需要
    重写绘制函数。

    这些五官都控制在一个 28px 格子内部。眼睛放在靠后的两侧，鼻孔放在前端，
    嘴巴画成短线并稍微离开边缘，这样能看出朝向，又不会让蛇头显得拥挤。
*/
static void DrawSnakeHead(SnakeGridPosition position, SnakeDirection direction, SnakeColorMode colorMode)
{
    /* 蛇头也从网格坐标转换为屏幕矩形，但比身体稍大一点以突出头部。 */
    Rectangle headRect = GridCellToRectangle(position.x, position.y);
    headRect.x += 2.0f;
    headRect.y += 2.0f;
    headRect.width -= 4.0f;
    headRect.height -= 4.0f;

    /* 先画蛇头的底色和描边，五官会覆盖在这个圆角矩形上。 */
    DrawRectangleRounded(headRect, 0.30f, 10, kSnakeHeadColor);
    DrawRectangleRoundedLines(headRect, 0.30f, 10, Fade(kTitleColor, 0.24f));

    /* 默认先按“向右”摆放五官；如果方向不是向右，下面的 switch 会覆盖这些位置。 */
    Vector2 firstEye = { headRect.x + headRect.width * 0.66f, headRect.y + headRect.height * 0.32f };
    Vector2 secondEye = { headRect.x + headRect.width * 0.66f, headRect.y + headRect.height * 0.68f };
    Vector2 firstNostril = { headRect.x + headRect.width * 0.82f, headRect.y + headRect.height * 0.43f };
    Vector2 secondNostril = { headRect.x + headRect.width * 0.82f, headRect.y + headRect.height * 0.57f };
    Vector2 mouthStart = { headRect.x + headRect.width * 0.69f, headRect.y + headRect.height * 0.50f };
    Vector2 mouthEnd = { headRect.x + headRect.width * 0.78f, headRect.y + headRect.height * 0.50f };

    /* 根据蛇头方向重新摆放五官，确保玩家能从画面上看出当前移动方向。 */
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

    /* 最后按计算好的位置画眼睛、鼻孔和嘴巴，完成蛇头朝向提示。 */
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

    /* 从蛇尾画到蛇头，保证蛇头最后绘制，视觉上不会被身体盖住。 */
    for (int i = renderData->segmentCount - 1; i >= 0; --i)
    {
        const SnakeSegmentRenderData* segment = &renderData->segments[i];

        /* 第 0 节在构建快照时被标记为蛇头，因此这里使用专门的蛇头绘制函数。 */
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
    DrawText("Phase 3 Render", PANEL_X + 24, PANEL_Y + 64, 18, kMutedTextColor);

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
    /* 开启垂直同步，减少窗口刷新时的画面撕裂。 */
    SetConfigFlags(FLAG_VSYNC_HINT);

    /* 创建固定尺寸窗口；窗口尺寸来自棋盘、边距和右侧信息栏的组合。 */
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SnakeGame - Phase 4 Movement");

    /* 界面每秒绘制 60 次，蛇的实际移动速度由 MOVE_INTERVAL_SECONDS 控制。 */
    SetTargetFPS(60);

    SnakeGameCore game;

    /* 初始化游戏核心；如果蛇身链表创建失败，直接关闭窗口并返回错误码。 */
    if (!SnakeGameInit(&game))
    {
        CloseWindow();
        return 1;
    }

    /* 计时器用于把 60 FPS 的绘制循环转换成 180ms 一格的蛇移动节奏。 */
    float moveTimer = 0.0f;

    while (!WindowShouldClose())
    {
        /* 每帧先读取输入，让玩家刚按下的方向尽快进入核心层。 */
        HandleDirectionInput(&game);

        /* 累加本帧耗时；只有累计超过移动间隔时，蛇才真正前进一步。 */
        moveTimer += GetFrameTime();
        if (moveTimer >= MOVE_INTERVAL_SECONDS)
        {
            /* 单步推进由核心层完成，界面层不直接修改蛇身链表。 */
            SnakeGameStep(&game);
            moveTimer = 0.0f;
        }

        /* BeginDrawing 和 EndDrawing 之间只做本帧绘制，不改变核心规则。 */
        BeginDrawing();
        ClearBackground(kWindowBackground);

        DrawBoardBackground();

        /* 从核心层构建只读渲染快照，构建成功后再绘制蛇。 */
        SnakeRenderData snakeRenderData;
        if (BuildSnakeRenderData(&game, &snakeRenderData))
        {
            DrawSnake(&snakeRenderData);
        }
        DrawInfoPanel();

        EndDrawing();
    }

    /* 退出窗口前释放核心层持有的蛇身链表资源。 */
    SnakeGameDestroy(&game);
    CloseWindow();
    return 0;
}
