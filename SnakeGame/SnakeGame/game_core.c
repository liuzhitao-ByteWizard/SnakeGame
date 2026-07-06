#include "game_core.h"

#include <stddef.h>
#include <time.h>

static SnakeGridPosition GetInitialHeadPosition(void)
{

    SnakeGridPosition head = {
        SNAKE_GAME_GRID_COLUMNS / 2,
        SNAKE_GAME_GRID_ROWS / 2
    };
    return head;
}

static SnakeGridPosition GetNextHeadPosition(SnakeGridPosition head, SnakeDirection direction)
{

    switch (direction)
    {
    case SNAKE_DIRECTION_UP:
        --head.y;
        break;
    case SNAKE_DIRECTION_DOWN:
        ++head.y;
        break;
    case SNAKE_DIRECTION_LEFT:
        --head.x;
        break;
    case SNAKE_DIRECTION_RIGHT:
    default:
        ++head.x;
        break;
    }

    return head;
}

static bool AreSamePosition(SnakeGridPosition first, SnakeGridPosition second)
{

    return first.x == second.x && first.y == second.y;
}

static bool IsOppositeDirection(SnakeDirection currentDirection, SnakeDirection requestedDirection)
{

    return (currentDirection == SNAKE_DIRECTION_UP && requestedDirection == SNAKE_DIRECTION_DOWN) ||
        (currentDirection == SNAKE_DIRECTION_DOWN && requestedDirection == SNAKE_DIRECTION_UP) ||
        (currentDirection == SNAKE_DIRECTION_LEFT && requestedDirection == SNAKE_DIRECTION_RIGHT) ||
        (currentDirection == SNAKE_DIRECTION_RIGHT && requestedDirection == SNAKE_DIRECTION_LEFT);
}

static bool IsInsideMap(const SnakeGameCore* game, SnakeGridPosition position)
{

    return position.x >= 0 &&
        position.y >= 0 &&
        position.x < game->mapColumns &&
        position.y < game->mapRows;
}

static int CalculateMoveIntervalMs(int level)
{

    int interval = SNAKE_GAME_INITIAL_MOVE_INTERVAL_MS -
        (level - 1) * SNAKE_GAME_MOVE_INTERVAL_STEP_MS;

    if (interval < SNAKE_GAME_MIN_MOVE_INTERVAL_MS)
    {
        interval = SNAKE_GAME_MIN_MOVE_INTERVAL_MS;
    }

    return interval;
}

static int CalculateTargetFoodCount(int level)
{

    int targetCount = SNAKE_GAME_INITIAL_FOOD_COUNT + level / SNAKE_GAME_FOOD_GROWTH_LEVEL_INTERVAL;

    if (targetCount > SNAKE_GAME_MAX_FOOD_COUNT)
    {
        targetCount = SNAKE_GAME_MAX_FOOD_COUNT;
    }

    return targetCount;
}

static void UpdateLevelSpeedAndFoodTarget(SnakeGameCore* game)
{

    game->level = game->foodsEaten / SNAKE_GAME_FOODS_PER_LEVEL + 1;
    game->moveIntervalMs = CalculateMoveIntervalMs(game->level);
    game->targetFoodCount = CalculateTargetFoodCount(game->level);
}

static unsigned int NextRandomValue(SnakeGameCore* game)
{

    game->randomState = game->randomState * 1664525u + 1013904223u;
    return game->randomState;
}

static bool IsPositionOnSnake(const SnakeGameCore* game, SnakeGridPosition position)
{
    if (game == NULL || game->snakeBody == NULL)
    {
        return false;
    }


    for (const DCListNode* segment = game->snakeBody->next; segment != game->snakeBody; segment = segment->next)
    {
        if (AreSamePosition(segment->data, position))
        {
            return true;
        }
    }

    return false;
}

static int FindFoodIndex(const SnakeGameCore* game, SnakeGridPosition position)
{
    if (game == NULL)
    {
        return -1;
    }

    for (int i = 0; i < game->foodCount; ++i)
    {
        if (AreSamePosition(game->foods[i], position))
        {
            return i;
        }
    }

    return -1;
}

static bool IsPositionOnFood(const SnakeGameCore* game, SnakeGridPosition position)
{
    return FindFoodIndex(game, position) >= 0;
}

static bool IsEmptyFoodCell(const SnakeGameCore* game, SnakeGridPosition position)
{

    return IsInsideMap(game, position) &&
        !IsPositionOnSnake(game, position) &&
        !IsPositionOnFood(game, position);
}

static int CountEmptyFoodCells(const SnakeGameCore* game)
{

    int emptyCellCount = 0;

    for (int y = 0; y < game->mapRows; ++y)
    {
        for (int x = 0; x < game->mapColumns; ++x)
        {
            const SnakeGridPosition candidate = { x, y };
            if (IsEmptyFoodCell(game, candidate))
            {
                ++emptyCellCount;
            }
        }
    }

    return emptyCellCount;
}

static bool GetEmptyFoodCellByIndex(const SnakeGameCore* game, int targetIndex, SnakeGridPosition* outPosition)
{

    int currentIndex = 0;

    for (int y = 0; y < game->mapRows; ++y)
    {
        for (int x = 0; x < game->mapColumns; ++x)
        {
            const SnakeGridPosition candidate = { x, y };
            if (!IsEmptyFoodCell(game, candidate))
            {
                continue;
            }

            if (currentIndex == targetIndex)
            {
                *outPosition = candidate;
                return true;
            }

            ++currentIndex;
        }
    }

    return false;
}

static bool AddRandomFood(SnakeGameCore* game)
{
    if (game->foodCount >= SNAKE_GAME_MAX_FOOD_COUNT)
    {
        return false;
    }


    const int emptyCellCount = CountEmptyFoodCells(game);
    if (emptyCellCount <= 0)
    {
        return false;
    }


    const int chosenIndex = (int)(NextRandomValue(game) % (unsigned int)emptyCellCount);
    SnakeGridPosition foodPosition;
    if (!GetEmptyFoodCellByIndex(game, chosenIndex, &foodPosition))
    {
        return false;
    }

    game->foods[game->foodCount] = foodPosition;
    ++game->foodCount;
    return true;
}

static void FillFoodsToTarget(SnakeGameCore* game)
{

    while (game->foodCount < game->targetFoodCount && game->foodCount < SNAKE_GAME_MAX_FOOD_COUNT)
    {
        if (!AddRandomFood(game))
        {
            break;
        }
    }
}

static void RemoveFoodAt(SnakeGameCore* game, int foodIndex)
{
    if (foodIndex < 0 || foodIndex >= game->foodCount)
    {
        return;
    }


    for (int i = foodIndex; i < game->foodCount - 1; ++i)
    {
        game->foods[i] = game->foods[i + 1];
    }

    --game->foodCount;
}
static bool DoesNextHeadHitSnakeBody(const SnakeGameCore* game, SnakeGridPosition nextHead)
{

    const DCListNode* sentinel = game->snakeBody;
    const DCListNode* oldTail = sentinel->prev;


    for (const DCListNode* segment = sentinel->next; segment != sentinel; segment = segment->next)
    {

        if (segment == oldTail)
        {
            continue;
        }


        if (AreSamePosition(segment->data, nextHead))
        {
            return true;
        }
    }

    return false;
}

static void BuildInitialSnake(DCListNode* snakeBody)
{
    /*
        按“蛇头 -> 身体 -> 蛇尾”的顺序建立初始链表。

        双向循环链表非常适合贪吃蛇，是因为它可以通过哨兵节点快速拿到两端：
        - sentinel->next 是蛇头，后续移动时会在这里插入新节点；
        - sentinel->prev 是蛇尾，普通移动时会从这里删除旧节点。

        也就是说，蛇的移动不是把每一节身体都复制一遍，而是只操作链表头尾。
        这正是本项目用贪吃蛇练习链表的核心原因。
    */
    const SnakeGridPosition head = GetInitialHeadPosition();

    for (int i = 0; i < SNAKE_GAME_INITIAL_LENGTH; ++i)
    {
        SnakeGridPosition segment = {
            head.x - i,
            head.y
        };

        DCListPushBack(snakeBody, segment);
    }
}

bool SnakeGameInit(SnakeGameCore* game)
{
    return SnakeGameInitWithSeed(game, (unsigned int)time(NULL));
}

bool SnakeGameInitWithSeed(SnakeGameCore* game, unsigned int seed)
{
    if (game == NULL)
    {
        return false;
    }


    game->mapColumns = SNAKE_GAME_GRID_COLUMNS;
    game->mapRows = SNAKE_GAME_GRID_ROWS;


    game->snakeLength = 0;
    game->direction = SNAKE_DIRECTION_RIGHT;
    game->status = SNAKE_GAME_STATUS_READY;
    game->snakeColorMode = SNAKE_COLOR_MODE_GREEN;
    game->score = 0;
    game->foodsEaten = 0;
    game->level = 1;
    game->moveIntervalMs = SNAKE_GAME_INITIAL_MOVE_INTERVAL_MS;
    game->targetFoodCount = SNAKE_GAME_INITIAL_FOOD_COUNT;
    game->foodCount = 0;
    game->randomState = seed;

    for (int i = 0; i < SNAKE_GAME_MAX_FOOD_COUNT; ++i)
    {
        game->foods[i].x = -1;
        game->foods[i].y = -1;
    }

    game->snakeBody = DCListInit();
    BuildInitialSnake(game->snakeBody);
    game->snakeLength = DCListSize(game->snakeBody);

    UpdateLevelSpeedAndFoodTarget(game);
    FillFoodsToTarget(game);

    game->status = SNAKE_GAME_STATUS_READY;

    return game->snakeLength == SNAKE_GAME_INITIAL_LENGTH &&
        game->foodCount == SNAKE_GAME_INITIAL_FOOD_COUNT;
}
bool SnakeGameStart(SnakeGameCore* game)
{
    if (game == NULL || game->snakeBody == NULL)
    {
        return false;
    }

    /*
        READY 表示“棋盘、蛇身、食物都已经准备好”，但游戏计时还没有开始。

        这样开始界面可以直接显示真实的蛇和苹果；玩家按 Enter 后，这里才把状态切到
        RUNNING。这个入口单独存在，测试就能清楚验证“初始化不会自动移动”。
    */
    if (game->status == SNAKE_GAME_STATUS_READY)
    {
        game->status = SNAKE_GAME_STATUS_RUNNING;
    }

    return true;
}

bool SnakeGameTogglePause(SnakeGameCore* game)
{
    if (game == NULL)
    {
        return false;
    }

    /*
        暂停逻辑故意放在核心层，而不是只放在 main.c。

        RUNNING 时，每个移动节拍都会演示链表操作：头插新蛇头，必要时尾删旧蛇尾。
        PAUSED 时，这条链表必须保持完全不变，所以 SnakeGameStep 会在非 RUNNING 状态
        直接返回，不执行任何节点插入或删除。
    */
    if (game->status == SNAKE_GAME_STATUS_RUNNING)
    {
        game->status = SNAKE_GAME_STATUS_PAUSED;
    }
    else if (game->status == SNAKE_GAME_STATUS_PAUSED)
    {
        game->status = SNAKE_GAME_STATUS_RUNNING;
    }

    return true;
}

bool SnakeGameRestart(SnakeGameCore* game)
{
    if (game == NULL)
    {
        return false;
    }

    /*
        重开不手工“修补”旧状态，而是走和新开一局相同的流程。

        先销毁旧链表，再重新创建哨兵节点和三节初始蛇身，可以保证链表所有权清晰：
        谁初始化，谁销毁；重开后不会混入上一局遗留的身体节点或食物状态。
    */
    SnakeGameDestroy(game);

    if (!SnakeGameInit(game))
    {
        return false;
    }

    return SnakeGameStart(game);
}
void SnakeGameDestroy(SnakeGameCore* game)
{
    if (game == NULL)
    {
        return;
    }

    DCListDestroy(game->snakeBody);

    game->snakeBody = NULL;

    game->snakeLength = 0;
    game->status = SNAKE_GAME_STATUS_READY;
    game->snakeColorMode = SNAKE_COLOR_MODE_GREEN;
    game->score = 0;
    game->foodsEaten = 0;
    game->level = 0;
    game->moveIntervalMs = 0;
    game->targetFoodCount = 0;
    game->foodCount = 0;
    game->randomState = 0;

    for (int i = 0; i < SNAKE_GAME_MAX_FOOD_COUNT; ++i)
    {
        game->foods[i].x = -1;
        game->foods[i].y = -1;
    }
}

bool SnakeGameStep(SnakeGameCore* game)
{
    if (game == NULL || game->snakeBody == NULL || game->snakeLength <= 0)
    {
        return false;
    }

    /*
        只有 RUNNING 状态才允许推进链表。

        READY：显示准备好的棋盘，但等待玩家按 Enter；
        PAUSED：冻结当前头尾顺序，不头插也不尾删；
        DEAD：保留死亡瞬间的蛇形，让结束界面能显示最终结果。
    */
    if (game->status != SNAKE_GAME_STATUS_RUNNING)
    {
        return true;
    }

    SnakeGridPosition currentHead;
    if (!SnakeGameGetSnakeHead(game, &currentHead))
    {
        return false;
    }

    const SnakeGridPosition nextHead = GetNextHeadPosition(currentHead, game->direction);

    if (!IsInsideMap(game, nextHead))
    {
        game->status = SNAKE_GAME_STATUS_DEAD;
        return true;
    }

    if (DoesNextHeadHitSnakeBody(game, nextHead))
    {
        game->status = SNAKE_GAME_STATUS_DEAD;
        return true;
    }

    const int foodIndex = FindFoodIndex(game, nextHead);
    const bool ateFood = foodIndex >= 0;

    /*
        这是本项目最重要的链表示例。

        贪吃蛇向前走一格时，并不需要把中间每一节身体都移动一次。更简单的做法是：
        1. 先把 nextHead 作为新节点插入链表头部，它就是新的蛇头；
        2. 如果没有吃到食物，就删除链表尾部节点，长度保持不变；
        3. 如果吃到食物，就保留旧尾巴，链表长度自然增加 1。

        “头插 + 尾删”正好对应“蛇头前进 + 蛇尾跟上”，这就是链表适合表示蛇身的原因。
    */
    DCListPushFront(game->snakeBody, nextHead);

    if (ateFood)
    {
        RemoveFoodAt(game, foodIndex);

        game->snakeColorMode = SNAKE_COLOR_MODE_RED;

        ++game->score;
        ++game->foodsEaten;
        UpdateLevelSpeedAndFoodTarget(game);

        game->snakeLength = DCListSize(game->snakeBody);
        FillFoodsToTarget(game);
    }
    else
    {
        DCListPopBack(game->snakeBody);
        game->snakeLength = DCListSize(game->snakeBody);
    }

    return true;
}
int SnakeGameGetMapColumns(const SnakeGameCore* game)
{
    return game ? game->mapColumns : 0;
}

int SnakeGameGetMapRows(const SnakeGameCore* game)
{
    return game ? game->mapRows : 0;
}

int SnakeGameGetSnakeLength(const SnakeGameCore* game)
{
    return game ? game->snakeLength : 0;
}

SnakeDirection SnakeGameGetDirection(const SnakeGameCore* game)
{
    return game ? game->direction : SNAKE_DIRECTION_RIGHT;
}

SnakeGameStatus SnakeGameGetStatus(const SnakeGameCore* game)
{
    return game ? game->status : SNAKE_GAME_STATUS_READY;
}
SnakeColorMode SnakeGameGetSnakeColorMode(const SnakeGameCore* game)
{
    return game ? game->snakeColorMode : SNAKE_COLOR_MODE_GREEN;
}
int SnakeGameGetScore(const SnakeGameCore* game)
{
    return game ? game->score : 0;
}

int SnakeGameGetFoodsEaten(const SnakeGameCore* game)
{
    return game ? game->foodsEaten : 0;
}

int SnakeGameGetLevel(const SnakeGameCore* game)
{
    return game ? game->level : 0;
}

int SnakeGameGetMoveIntervalMs(const SnakeGameCore* game)
{
    return game ? game->moveIntervalMs : 0;
}

int SnakeGameGetFoodCount(const SnakeGameCore* game)
{
    return game ? game->foodCount : 0;
}

int SnakeGameGetTargetFoodCount(const SnakeGameCore* game)
{
    return game ? game->targetFoodCount : 0;
}

bool SnakeGameGetFoodPosition(const SnakeGameCore* game, int index, SnakeGridPosition* outPosition)
{
    if (game == NULL || outPosition == NULL || index < 0 || index >= game->foodCount)
    {
        return false;
    }

    *outPosition = game->foods[index];
    return true;
}
bool SnakeGameRequestDirection(SnakeGameCore* game, SnakeDirection requestedDirection)
{
    if (game == NULL)
    {
        return false;
    }


    if (IsOppositeDirection(game->direction, requestedDirection))
    {
        return true;
    }


    game->direction = requestedDirection;
    return true;
}

bool SnakeGameGetSnakeHead(const SnakeGameCore* game, SnakeGridPosition* outPosition)
{

    return SnakeGameGetSnakeSegment(game, 0, outPosition);
}

bool SnakeGameGetSnakeTail(const SnakeGameCore* game, SnakeGridPosition* outPosition)
{

    if (game == NULL || outPosition == NULL || game->snakeBody == NULL || game->snakeLength <= 0)
    {
        return false;
    }


    const DCListNode* tail = game->snakeBody->prev;


    if (tail == game->snakeBody)
    {
        return false;
    }


    *outPosition = tail->data;
    return true;
}

bool SnakeGameGetSnakeSegment(const SnakeGameCore* game, int index, SnakeGridPosition* outPosition)
{

    if (game == NULL || outPosition == NULL || game->snakeBody == NULL)
    {
        return false;
    }


    if (index < 0 || index >= game->snakeLength)
    {
        return false;
    }


    const DCListNode* segment = DCListGetConstElem(game->snakeBody, index);
    *outPosition = segment->data;
    return true;
}
