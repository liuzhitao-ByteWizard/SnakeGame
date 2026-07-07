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

/* 定义吃食物音效文件名，界面层只关心播放声音，不把声音概念放进核心规则。 */
#define SOUND_FILE_EAT "eat.wav"

/* 定义升级音效文件名，升级事件由核心层的等级变化推导出来。 */
#define SOUND_FILE_LEVEL "level.wav"

/* 定义死亡音效文件名，死亡事件由核心层的状态变化推导出来。 */
#define SOUND_FILE_DEATH "death.wav"

/* 定义吃食物反馈时长，让闪光和蛇头缩放能在短时间内自然消失。 */
#define EAT_FEEDBACK_DURATION 0.26f

/* 定义升级提示时长，让玩家能看到升级但不会挡住操作太久。 */
#define LEVEL_FEEDBACK_DURATION 0.90f

/* 定义死亡反馈时长，让红色闪烁和棋盘抖动集中在死亡瞬间。 */
#define DEATH_FEEDBACK_DURATION 0.55f

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

/* 保存界面层短暂反馈状态，核心层只负责规则结果，闪光和抖动留给 raylib 界面层处理。 */
typedef struct UiFeedbackState
{
    float eatTimer;
    float levelTimer;
    float deathTimer;
    SnakeGridPosition eatPosition;
    bool hasEatPosition;
} UiFeedbackState;

/* 保存已经加载好的音效，加载失败时只关闭对应开关，游戏流程继续正常运行。 */
typedef struct UiSoundBank
{
    Sound eatSound;
    Sound levelSound;
    Sound deathSound;
    bool eatLoaded;
    bool levelLoaded;
    bool deathLoaded;
} UiSoundBank;


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

static float Clamp01(float value)
{
    /* 小于 0 的透明度和比例没有绘制意义，所以先压到 0。 */
    if (value < 0.0f)
    {
        return 0.0f;
    }

    /* 大于 1 的反馈进度会让颜色过亮，所以再压到 1。 */
    if (value > 1.0f)
    {
        return 1.0f;
    }

    /* 正常范围内的值可以直接返回。 */
    return value;
}

static void ResetUiFeedback(UiFeedbackState* feedback)
{
    /* 界面反馈状态只影响视觉和声音，重开或开始时清零不会改变核心规则。 */
    if (feedback == NULL)
    {
        return;
    }

    /* 清空吃食物反馈计时，避免上一局的闪光带到新局。 */
    feedback->eatTimer = 0.0f;

    /* 清空升级提示计时，避免上一局的升级文字残留。 */
    feedback->levelTimer = 0.0f;

    /* 清空死亡反馈计时，避免重开后棋盘继续抖动。 */
    feedback->deathTimer = 0.0f;

    /* 默认没有可绘制的吃食物位置。 */
    feedback->eatPosition = (SnakeGridPosition){ 0, 0 };

    /* 标记当前没有吃食物闪光点。 */
    feedback->hasEatPosition = false;
}

static void UpdateUiFeedback(UiFeedbackState* feedback, float deltaTime)
{
    /* 反馈计时属于界面动画，每帧按真实时间衰减，不影响蛇的一格一格移动规则。 */
    if (feedback == NULL)
    {
        return;
    }

    /* 吃食物计时大于 0 时才需要递减。 */
    if (feedback->eatTimer > 0.0f)
    {
        /* 用帧时间递减，让反馈时长不依赖机器帧率。 */
        feedback->eatTimer -= deltaTime;

        /* 计时结束后清理吃食物位置，绘制函数就不会再画闪光。 */
        if (feedback->eatTimer <= 0.0f)
        {
            feedback->eatTimer = 0.0f;
            feedback->hasEatPosition = false;
        }
    }

    /* 升级提示计时大于 0 时才需要递减。 */
    if (feedback->levelTimer > 0.0f)
    {
        /* 用同一套帧时间递减，保证所有界面反馈节奏一致。 */
        feedback->levelTimer -= deltaTime;

        /* 计时结束后压回 0，避免浮点误差留下很小的负数。 */
        if (feedback->levelTimer < 0.0f)
        {
            feedback->levelTimer = 0.0f;
        }
    }

    /* 死亡反馈计时大于 0 时才需要递减。 */
    if (feedback->deathTimer > 0.0f)
    {
        /* 死亡抖动和红色覆盖也按真实时间衰减。 */
        feedback->deathTimer -= deltaTime;

        /* 计时结束后压回 0，让结束界面保持稳定。 */
        if (feedback->deathTimer < 0.0f)
        {
            feedback->deathTimer = 0.0f;
        }
    }
}

static void TriggerEatFeedback(UiFeedbackState* feedback, SnakeGridPosition position)
{
    /* 吃食物反馈由分数变化触发，界面层只记录要闪光的格子。 */
    if (feedback == NULL)
    {
        return;
    }

    /* 记录吃到食物后的蛇头位置，闪光会围绕这个格子绘制。 */
    feedback->eatPosition = position;

    /* 标记当前有可绘制的吃食物反馈位置。 */
    feedback->hasEatPosition = true;

    /* 重置吃食物计时，让连续吃食物时反馈从最亮状态重新开始。 */
    feedback->eatTimer = EAT_FEEDBACK_DURATION;
}

static void TriggerLevelFeedback(UiFeedbackState* feedback)
{
    /* 升级反馈由等级变化触发，核心层不用知道界面上会显示提示文字。 */
    if (feedback == NULL)
    {
        return;
    }

    /* 重置升级计时，让 LEVEL UP 提示完整显示一小段时间。 */
    feedback->levelTimer = LEVEL_FEEDBACK_DURATION;
}

static void TriggerDeathFeedback(UiFeedbackState* feedback)
{
    /* 死亡反馈由状态切换触发，核心层只暴露 DEAD 状态，红屏和抖动留在界面层。 */
    if (feedback == NULL)
    {
        return;
    }

    /* 重置死亡计时，让死亡瞬间有明确的红色和抖动反馈。 */
    feedback->deathTimer = DEATH_FEEDBACK_DURATION;
}

static bool TryLoadUiSound(const char* fileName, Sound* outSound)
{
    /* 声音路径只在界面层尝试，核心层不依赖 raylib，也不依赖文件系统。 */
    static const char* pathPrefixes[] = {
        "../Sound/",
        "Sound/",
        "../../Sound/"
    };

    /* 检查参数，避免加载失败时写入无效地址。 */
    if (fileName == NULL || outSound == NULL)
    {
        return false;
    }

    /* 按常见工作目录顺序尝试音效路径，适配 Visual Studio 和直接运行。 */
    for (int i = 0; i < (int)(sizeof(pathPrefixes) / sizeof(pathPrefixes[0])); ++i)
    {
        /* 组合当前候选路径，路径短小固定，缓冲区足够容纳项目内资源路径。 */
        char soundPath[256];

        /* 把目录前缀和文件名拼成 raylib 可以读取的相对路径。 */
        snprintf(soundPath, sizeof(soundPath), "%s%s", pathPrefixes[i], fileName);

        /* 只有文件存在时才调用 LoadSound，避免 raylib 反复打印找不到文件的日志。 */
        if (FileExists(soundPath))
        {
            /* 加载当前候选音效文件。 */
            Sound loadedSound = LoadSound(soundPath);

            /* raylib 提供有效性检查，避免把坏音频保存进音效库。 */
            if (IsSoundValid(loadedSound))
            {
                *outSound = loadedSound;
                return true;
            }
        }
    }

    /* 所有路径都失败时返回 false，调用方会静默跳过对应音效。 */
    return false;
}

static UiSoundBank LoadUiSounds(void)
{
    /* 默认把所有音效标记为未加载，音频设备不可用时游戏仍然可以玩。 */
    UiSoundBank sounds = { 0 };

    /* 只有音频设备可用时才加载声音，避免无声设备环境下初始化失败影响游戏。 */
    if (!IsAudioDeviceReady())
    {
        return sounds;
    }

    /* 加载吃食物音效，失败时 eatLoaded 保持 false。 */
    sounds.eatLoaded = TryLoadUiSound(SOUND_FILE_EAT, &sounds.eatSound);

    /* 加载升级音效，失败时 levelLoaded 保持 false。 */
    sounds.levelLoaded = TryLoadUiSound(SOUND_FILE_LEVEL, &sounds.levelSound);

    /* 加载死亡音效，失败时 deathLoaded 保持 false。 */
    sounds.deathLoaded = TryLoadUiSound(SOUND_FILE_DEATH, &sounds.deathSound);

    /* 返回已经加载好的音效句柄和加载状态。 */
    return sounds;
}

static void UnloadUiSounds(UiSoundBank* sounds)
{
    /* 卸载函数集中释放 raylib 音效资源，避免退出游戏时留下音频缓冲。 */
    if (sounds == NULL)
    {
        return;
    }

    /* 只有加载成功的吃食物音效才需要卸载。 */
    if (sounds->eatLoaded)
    {
        UnloadSound(sounds->eatSound);
        sounds->eatLoaded = false;
    }

    /* 只有加载成功的升级音效才需要卸载。 */
    if (sounds->levelLoaded)
    {
        UnloadSound(sounds->levelSound);
        sounds->levelLoaded = false;
    }

    /* 只有加载成功的死亡音效才需要卸载。 */
    if (sounds->deathLoaded)
    {
        UnloadSound(sounds->deathSound);
        sounds->deathLoaded = false;
    }
}

static void PlayEatSound(const UiSoundBank* sounds)
{
    /* 声音播放只跟界面事件绑定，核心层不用知道玩家听到了什么。 */
    if (sounds != NULL && sounds->eatLoaded)
    {
        PlaySound(sounds->eatSound);
    }
}

static void PlayLevelSound(const UiSoundBank* sounds)
{
    /* 升级音效只在等级发生变化时播放一次。 */
    if (sounds != NULL && sounds->levelLoaded)
    {
        PlaySound(sounds->levelSound);
    }
}

static void PlayDeathSound(const UiSoundBank* sounds)
{
    /* 死亡音效只在状态刚切到 DEAD 时播放一次。 */
    if (sounds != NULL && sounds->deathLoaded)
    {
        PlaySound(sounds->deathSound);
    }
}

static float GetSnakeHeadPulseScale(const UiFeedbackState* feedback)
{
    /* 没有吃食物反馈时使用正常大小，保持经典一格一格移动的观感。 */
    if (feedback == NULL || feedback->eatTimer <= 0.0f)
    {
        return 1.0f;
    }

    /* 反馈剩余时间越多，蛇头越接近最大放大比例。 */
    const float progress = Clamp01(feedback->eatTimer / EAT_FEEDBACK_DURATION);

    /* 放大幅度控制得很轻，避免蛇头盖住太多相邻格子。 */
    return 1.0f + progress * 0.16f;
}

static Vector2 GetBoardShakeOffset(const UiFeedbackState* feedback)
{
    /* 默认不抖动，普通游戏过程保持棋盘稳定。 */
    Vector2 offset = { 0.0f, 0.0f };

    /* 只有死亡反馈计时存在时才计算抖动偏移。 */
    if (feedback == NULL || feedback->deathTimer <= 0.0f)
    {
        return offset;
    }

    /* 死亡反馈逐渐衰减，让抖动从明显变成稳定。 */
    const float progress = Clamp01(feedback->deathTimer / DEATH_FEEDBACK_DURATION);

    /* 用计时值切换方向，得到轻微但清楚的撞击感。 */
    const int shakeStep = (int)(feedback->deathTimer * 80.0f);

    /* 根据奇偶步决定水平抖动方向。 */
    const float directionX = (shakeStep % 2 == 0) ? 1.0f : -1.0f;

    /* 根据三步节奏决定垂直抖动方向。 */
    const float directionY = (shakeStep % 3 == 0) ? 1.0f : -1.0f;

    /* 把抖动限制在少量像素内，增强死亡反馈但不破坏布局。 */
    offset.x = directionX * 4.0f * progress;
    offset.y = directionY * 2.0f * progress;

    /* 返回给主循环中的相机偏移使用。 */
    return offset;
}

static void DrawEatFeedbackFlash(const UiFeedbackState* feedback)
{
    /* 没有有效吃食物位置时不绘制闪光。 */
    if (feedback == NULL || !feedback->hasEatPosition || feedback->eatTimer <= 0.0f)
    {
        return;
    }

    /* 把吃到食物的格子转换成屏幕矩形，闪光就能跟蛇头对齐。 */
    const Rectangle cell = GridCellToRectangle(feedback->eatPosition.x, feedback->eatPosition.y);

    /* 计算当前反馈进度，用于控制透明度和圆环大小。 */
    const float progress = Clamp01(feedback->eatTimer / EAT_FEEDBACK_DURATION);

    /* 计算格子中心，圆环和亮点都围绕这个位置绘制。 */
    const Vector2 center = { cell.x + cell.width * 0.50f, cell.y + cell.height * 0.50f };

    /* 绘制向外扩散的绿色亮环，提示玩家刚刚吃到了食物。 */
    DrawRing(center, cell.width * 0.22f, cell.width * (0.52f - progress * 0.12f), 0.0f, 360.0f, 32, Fade(kAccentGreen, 0.58f * progress));

    /* 绘制中心短闪白光，让吃食物反馈更容易被看见。 */
    DrawCircleV(center, cell.width * 0.18f * progress, Fade(WHITE, 0.38f * progress));
}

static void DrawLevelFeedback(const UiFeedbackState* feedback)
{
    /* 没有升级计时时不绘制升级提示。 */
    if (feedback == NULL || feedback->levelTimer <= 0.0f)
    {
        return;
    }

    /* 使用剩余时间控制透明度，让文字自然淡出。 */
    const float progress = Clamp01(feedback->levelTimer / LEVEL_FEEDBACK_DURATION);

    /* 固定升级提示文字，避免在紧凑棋盘上产生额外布局变化。 */
    const char* text = "LEVEL UP";

    /* 先测量文字宽度，保证提示始终居中。 */
    const int fontSize = 34;

    /* 根据文字宽度计算居中位置。 */
    const int textWidth = MeasureText(text, fontSize);

    /* 提示放在棋盘上方区域，不遮挡右侧信息栏。 */
    const int textX = BOARD_X + (BOARD_WIDTH - textWidth) / 2;

    /* 提示略微靠上，玩家看得到但不会挡住蛇头附近操作。 */
    const int textY = BOARD_Y + 52;

    /* 先绘制轻微阴影，保证深色棋盘和红色覆盖下文字仍清晰。 */
    DrawText(text, textX + 2, textY + 2, fontSize, Fade(BLACK, 0.45f * progress));

    /* 再绘制主文字，用绿色强调升级成功。 */
    DrawText(text, textX, textY, fontSize, Fade(kAccentGreen, progress));
}

static void DrawDeathFeedback(const UiFeedbackState* feedback)
{
    /* 没有死亡计时时不绘制红色覆盖。 */
    if (feedback == NULL || feedback->deathTimer <= 0.0f)
    {
        return;
    }

    /* 使用剩余时间控制红色覆盖透明度，让死亡瞬间更明显。 */
    const float progress = Clamp01(feedback->deathTimer / DEATH_FEEDBACK_DURATION);

    /* 在棋盘范围内绘制红色覆盖，强调撞墙或自撞已经发生。 */
    DrawRectangle(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, Fade(kAppleRed, 0.24f * progress));

    /* 绘制一圈红色边框，让死亡反馈和普通暂停覆盖区分开。 */
    DrawRectangleLines(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, Fade(kAppleRed, 0.78f * progress));
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

static void HandleGameInput(SnakeGameCore* game, float* moveTimer, UiFeedbackState* feedback)
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
            /* 清空界面反馈，确保开始时没有上一轮残留的闪光或抖动。 */
            ResetUiFeedback(feedback);

            SnakeGameStart(game);
            *moveTimer = 0.0f;
        }
        else if (status == SNAKE_GAME_STATUS_DEAD)
        {
            /* 清空界面反馈，确保死亡后再来一局时画面立即恢复稳定。 */
            ResetUiFeedback(feedback);

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
            /* 清空界面反馈，确保游戏中重开不会继承当前局的反馈动画。 */
            ResetUiFeedback(feedback);

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


static void DrawSnakeHead(SnakeGridPosition position, SnakeDirection direction, SnakeColorMode colorMode, float pulseScale)
{

    Rectangle headRect = GridCellToRectangle(position.x, position.y);
    headRect.x += 2.0f;
    headRect.y += 2.0f;
    headRect.width -= 4.0f;
    headRect.height -= 4.0f;

    /* 根据吃食物反馈轻微放大蛇头，仍然保持一格一格跳动，不做平滑移动。 */
    const float pulseExtra = (pulseScale - 1.0f) * CELL_SIZE * 0.50f;

    /* 从中心向外扩展矩形，避免放大时蛇头偏向某一边。 */
    headRect.x -= pulseExtra;
    headRect.y -= pulseExtra;
    headRect.width += pulseExtra * 2.0f;
    headRect.height += pulseExtra * 2.0f;


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

static void DrawSnake(const SnakeRenderData* renderData, const UiFeedbackState* feedback)
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
            /* 只有蛇头参与吃食物缩放反馈，蛇身仍然按链表节点逐节绘制。 */
            const float pulseScale = GetSnakeHeadPulseScale(feedback);

            DrawSnakeHead(segment->position, renderData->direction, renderData->colorMode, pulseScale);
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
    DrawText("Phase 8 Polish", PANEL_X + 24, PANEL_Y + 64, 18, kMutedTextColor);

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

    /* 创建固定尺寸窗口，并在标题中标明当前 Phase 8 已接入反馈和音效打磨。 */
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SnakeGame - Phase 8 Polish");

    /* 初始化音频设备，音效属于 raylib 界面层资源，不进入 game_core。 */
    InitAudioDevice();

    /* 加载三种界面音效，失败时只跳过声音，不影响游戏玩法。 */
    UiSoundBank sounds = LoadUiSounds();

    /* 准备界面反馈状态，吃食物、升级、死亡都会通过这个状态绘制短动画。 */
    UiFeedbackState feedback;

    /* 初始化反馈状态，保证第一帧没有随机残留动画。 */
    ResetUiFeedback(&feedback);

    /* 设置目标帧率为 60 帧，移动节奏仍由核心层的毫秒间隔控制。 */
    SetTargetFPS(60);

    /* 创建游戏核心状态对象，蛇身、食物、分数等规则数据都放在这里。 */
    SnakeGameCore game;

    /* 初始化核心状态，准备初始蛇身、初始食物和 READY 状态。 */
    if (!SnakeGameInit(&game))
    {
        /* 如果核心状态初始化失败，先按创建顺序反向释放界面资源。 */
        /* 初始化失败时先释放已经尝试加载的音效资源。 */
        UnloadUiSounds(&sounds);

        /* 初始化失败时关闭音频设备，保证 raylib 资源成对释放。 */
        if (IsAudioDeviceReady())
        {
            CloseAudioDevice();
        }

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
        /* 读取本帧真实耗时，移动计时和反馈动画共用同一个时间来源。 */
        const float frameTime = GetFrameTime();

        /* 每帧衰减界面反馈计时，动画结束后自动停止绘制。 */
        UpdateUiFeedback(&feedback, frameTime);

        /* 先处理键盘输入，把 Enter、P、R 和方向键转换成核心层命令。 */
        HandleGameInput(&game, &moveTimer, &feedback);

        /* 只有 RUNNING 状态才推进蛇的移动。 */
        if (SnakeGameGetStatus(&game) == SNAKE_GAME_STATUS_RUNNING)
        {
            /* 把核心层给出的毫秒移动间隔换算成秒，方便和 GetFrameTime 相加比较。 */
            const float moveIntervalSeconds = (float)SnakeGameGetMoveIntervalMs(&game) / 1000.0f;

            /* 累加这一帧经过的时间。 */
            moveTimer += frameTime;

            /* 当累计时间达到移动间隔时，才让核心层前进一步。 */
            if (moveTimer >= moveIntervalSeconds)
            {
                /* 记录推进前的状态，用来判断这一帧是否刚好从运行进入死亡。 */
                const SnakeGameStatus statusBeforeStep = SnakeGameGetStatus(&game);

                /* 记录推进前的分数，用来判断这一帧是否刚好吃到食物。 */
                const int scoreBeforeStep = SnakeGameGetScore(&game);

                /* 记录推进前的等级，用来判断这一帧是否刚好升级。 */
                const int levelBeforeStep = SnakeGameGetLevel(&game);

                /* 推进游戏核心一步，内部会处理移动、吃食物、撞墙和自撞。 */
                SnakeGameStep(&game);

                /* 分数增加说明本帧吃到了食物，可以播放声音并绘制蛇头闪光。 */
                if (SnakeGameGetScore(&game) > scoreBeforeStep)
                {
                    /* 吃到食物后的蛇头位置就是刚才食物所在的位置。 */
                    SnakeGridPosition eatenPosition;

                    /* 只有成功取到蛇头位置时才绘制格子闪光。 */
                    if (SnakeGameGetSnakeHead(&game, &eatenPosition))
                    {
                        TriggerEatFeedback(&feedback, eatenPosition);
                    }

                    /* 播放吃食物音效，加载失败时函数会自动跳过。 */
                    PlayEatSound(&sounds);
                }

                /* 等级增加说明本帧刚好达到升级条件。 */
                if (SnakeGameGetLevel(&game) > levelBeforeStep)
                {
                    /* 触发棋盘上的升级提示文字。 */
                    TriggerLevelFeedback(&feedback);

                    /* 播放升级音效，加载失败时函数会自动跳过。 */
                    PlayLevelSound(&sounds);
                }

                /* 如果这一帧刚刚死亡，就检查是否需要刷新本地最高分。 */
                if (statusBeforeStep != SNAKE_GAME_STATUS_DEAD &&
                    SnakeGameGetStatus(&game) == SNAKE_GAME_STATUS_DEAD)
                {
                    /* 触发死亡红屏和棋盘抖动。 */
                    TriggerDeathFeedback(&feedback);

                    /* 播放死亡音效，加载失败时函数会自动跳过。 */
                    PlayDeathSound(&sounds);

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

        /* 根据死亡反馈计算棋盘抖动偏移，右侧信息栏不参与抖动。 */
        const Vector2 boardShakeOffset = GetBoardShakeOffset(&feedback);

        /* 创建只带偏移的相机，用来临时移动棋盘绘制内容。 */
        Camera2D boardCamera = { 0 };

        /* 设置相机偏移，死亡反馈结束后这里会自然回到零。 */
        boardCamera.offset = boardShakeOffset;

        /* 保持缩放为 1，避免改变网格尺寸。 */
        boardCamera.zoom = 1.0f;

        /* 开始棋盘相机绘制，只有地图、食物和蛇会跟着死亡反馈抖动。 */
        BeginMode2D(boardCamera);

        /* 绘制棋盘背景。 */
        DrawBoardBackground();

        /* 绘制当前所有食物。 */
        DrawFoods(&game);

        /* 准备蛇身渲染数据，把核心层链表数据转换成绘制层容易使用的数组。 */
        SnakeRenderData snakeRenderData;

        /* 如果蛇身渲染数据构建成功，就绘制整条蛇。 */
        if (BuildSnakeRenderData(&game, &snakeRenderData))
        {
            /* 死亡时把蛇临时绘制成红色，强调最终碰撞状态。 */
            if (SnakeGameGetStatus(&game) == SNAKE_GAME_STATUS_DEAD)
            {
                snakeRenderData.colorMode = SNAKE_COLOR_MODE_RED;
            }

            /* 绘制蛇头和蛇身。 */
            DrawSnake(&snakeRenderData, &feedback);
        }

        /* 在蛇头位置绘制吃食物闪光，让玩家立即看到得分反馈。 */
        DrawEatFeedbackFlash(&feedback);

        /* 结束棋盘相机绘制，后续覆盖层和信息栏保持稳定不抖动。 */
        EndMode2D();

        /* 绘制死亡红色覆盖，和棋盘抖动一起形成撞击反馈。 */
        DrawDeathFeedback(&feedback);

        /* 绘制升级提示文字，位置固定在棋盘区域内，不影响右侧信息栏。 */
        DrawLevelFeedback(&feedback);

        /* 根据 READY、PAUSED、DEAD 状态绘制覆盖层，并传入最高分用于显示。 */
        DrawStateOverlay(&game, highScore);

        /* 绘制右侧信息栏，并传入最高分用于显示。 */
        DrawInfoPanel(&game, highScore);

        /* 结束一帧绘制，把画面显示到窗口。 */
        EndDrawing();
    }

    /* 退出前释放界面音效资源。 */
    UnloadUiSounds(&sounds);

    /* 音频设备打开过才关闭，保持 raylib 音频生命周期成对。 */
    if (IsAudioDeviceReady())
    {
        CloseAudioDevice();
    }

    /* 程序退出前销毁核心层链表，释放蛇身节点。 */
    SnakeGameDestroy(&game);

    /* 关闭 raylib 窗口。 */
    CloseWindow();

    /* 返回 0，表示程序正常结束。 */
    return 0;
}
