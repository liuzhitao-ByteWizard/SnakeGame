#pragma once
#ifndef SNAKE_GAME_GAME_CORE_INCLUDED
#define SNAKE_GAME_GAME_CORE_INCLUDED

#include "DCList.h"
#include "grid_position.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SNAKE_GAME_GRID_COLUMNS 30
#define SNAKE_GAME_GRID_ROWS 20

/*
    游戏核心只关心“格子坐标”，不关心屏幕像素。

    这里把地图大小和初始蛇长放在核心层，是为了让界面层和测试层共用同一套规则：
    - raylib 界面层可以把 30 x 20 的格子换算成屏幕上的矩形；
    - Google Test 可以直接检查格子坐标，不需要打开窗口。
*/
#define SNAKE_GAME_INITIAL_LENGTH 3

/*
    食物、等级和速度都属于玩法规则，所以放在 game_core 中，而不是放在 main.c。

    界面层只负责显示这些值，不重新计算“应该有几个苹果”或“蛇应该多快移动”。
    这样做可以避免界面和核心各算一遍导致规则不一致，也方便单元测试直接验证核心逻辑。
*/
#define SNAKE_GAME_INITIAL_FOOD_COUNT 5
#define SNAKE_GAME_MAX_FOOD_COUNT 12
#define SNAKE_GAME_FOODS_PER_LEVEL 5
#define SNAKE_GAME_FOOD_GROWTH_LEVEL_INTERVAL 2
#define SNAKE_GAME_INITIAL_MOVE_INTERVAL_MS 180
#define SNAKE_GAME_MOVE_INTERVAL_STEP_MS 10
#define SNAKE_GAME_MIN_MOVE_INTERVAL_MS 80


typedef enum SnakeDirection
{
    SNAKE_DIRECTION_UP = 0,
    SNAKE_DIRECTION_RIGHT, //1
    SNAKE_DIRECTION_DOWN, //2
    SNAKE_DIRECTION_LEFT //3
} SnakeDirection;


typedef enum SnakeGameStatus
{
    SNAKE_GAME_STATUS_READY = 0,
    SNAKE_GAME_STATUS_RUNNING,
    SNAKE_GAME_STATUS_PAUSED,
    SNAKE_GAME_STATUS_DEAD
} SnakeGameStatus;

typedef enum SnakeColorMode
{
    SNAKE_COLOR_MODE_GREEN = 0,
    SNAKE_COLOR_MODE_RED
} SnakeColorMode;

typedef struct SnakeGameCore
{
    int mapColumns;
    int mapRows;
    int snakeLength;
    SnakeDirection direction;
    SnakeGameStatus status;
    SnakeColorMode snakeColorMode;

    /*
        蛇身必须只用双向循环链表保存。

        这个链表有一个哨兵节点：
        - snakeBody->next 指向蛇头；
        - snakeBody->prev 指向蛇尾。

        这正好对应贪吃蛇移动：
        - 每走一步，在链表头部插入一个“新蛇头”；
        - 如果没吃到食物，就从链表尾部删除“旧蛇尾”；
        - 如果吃到食物，就不删尾巴，蛇身自然增长一节。
    */
    DCListNode* snakeBody;

    int score;
    int foodsEaten;
    int level;
    int moveIntervalMs;
    int targetFoodCount;
    int foodCount;
    SnakeGridPosition foods[SNAKE_GAME_MAX_FOOD_COUNT];
    unsigned int randomState;
} SnakeGameCore;


bool SnakeGameInit(SnakeGameCore* game);


bool SnakeGameInitWithSeed(SnakeGameCore* game, unsigned int seed);

/*
    开始一局已经准备好的游戏。

    Phase 6 把“棋盘已经初始化”和“时间已经开始流动”拆开处理。
    SnakeGameInit 会提前创建初始蛇身和食物，因此开始界面能显示真实棋盘；
    但玩家按 Enter 之前，蛇不应该移动。这个函数就是 READY -> RUNNING 的明确入口。
*/
bool SnakeGameStart(SnakeGameCore* game);

/*
    在运行中的一局里切换暂停/继续。

    暂停不是单纯的界面遮罩，而是核心状态。原因是暂停时链表不能发生任何变化：
    不能头插新节点，也不能尾删旧节点。把规则放在核心层后，界面和测试看到的行为一致。
*/
bool SnakeGameTogglePause(SnakeGameCore* game);

/*
    重新开始当前一局，并直接进入 RUNNING。

    界面层会在游戏中按 R、死亡后按 Enter 时调用它。
    重开时走“销毁旧链表 -> 重新初始化 -> 开始运行”的统一流程，避免残留旧蛇身节点。
*/
bool SnakeGameRestart(SnakeGameCore* game);


void SnakeGameDestroy(SnakeGameCore* game);


bool SnakeGameStep(SnakeGameCore* game);


int SnakeGameGetMapColumns(const SnakeGameCore* game);
int SnakeGameGetMapRows(const SnakeGameCore* game);
int SnakeGameGetSnakeLength(const SnakeGameCore* game);
SnakeDirection SnakeGameGetDirection(const SnakeGameCore* game);
SnakeGameStatus SnakeGameGetStatus(const SnakeGameCore* game);
SnakeColorMode SnakeGameGetSnakeColorMode(const SnakeGameCore* game);

int SnakeGameGetScore(const SnakeGameCore* game);
int SnakeGameGetFoodsEaten(const SnakeGameCore* game);
int SnakeGameGetLevel(const SnakeGameCore* game);
int SnakeGameGetMoveIntervalMs(const SnakeGameCore* game);
int SnakeGameGetFoodCount(const SnakeGameCore* game);
int SnakeGameGetTargetFoodCount(const SnakeGameCore* game);

bool SnakeGameGetFoodPosition(const SnakeGameCore* game, int index, SnakeGridPosition* outPosition);

bool SnakeGameRequestDirection(SnakeGameCore* game, SnakeDirection requestedDirection);


bool SnakeGameGetSnakeHead(const SnakeGameCore* game, SnakeGridPosition* outPosition);


bool SnakeGameGetSnakeTail(const SnakeGameCore* game, SnakeGridPosition* outPosition);


bool SnakeGameGetSnakeSegment(const SnakeGameCore* game, int index, SnakeGridPosition* outPosition);

#ifdef __cplusplus
}
#endif

#endif
