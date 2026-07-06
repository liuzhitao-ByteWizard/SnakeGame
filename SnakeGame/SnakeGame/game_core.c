#include "game_core.h"

#include <stddef.h>
#include <time.h>

static SnakeGridPosition GetInitialHeadPosition(void)
{
    /*
        地图尺寸是 30 x 20。整数除法会把初始蛇头放在 (15, 10)，也就是
        规则中使用的地图中间格。初始方向向右，因此后续两节身体放在
        蛇头左侧：(14, 10) 和 (13, 10)。
    */
    SnakeGridPosition head = {
        SNAKE_GAME_GRID_COLUMNS / 2,
        SNAKE_GAME_GRID_ROWS / 2
    };
    return head;
}

static SnakeGridPosition GetNextHeadPosition(SnakeGridPosition head, SnakeDirection direction)
{
    /*
        Phase 2 只负责计算下一格坐标。后续阶段会在真正移动前补充碰撞检测，
        用来判断这个坐标是否合法。
    */
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
    /* 网格坐标只有 x 和 y 两个分量，两个分量都相同才表示落在同一个格子。 */
    return first.x == second.x && first.y == second.y;
}

static bool IsOppositeDirection(SnakeDirection currentDirection, SnakeDirection requestedDirection)
{
    /*
        只有上下、左右这两组方向互为反向。把这个判断集中到一个小函数里，
        SnakeGameRequestDirection 就不用把四种组合写散，后续读转向规则也更直接。
    */
    return (currentDirection == SNAKE_DIRECTION_UP && requestedDirection == SNAKE_DIRECTION_DOWN) ||
        (currentDirection == SNAKE_DIRECTION_DOWN && requestedDirection == SNAKE_DIRECTION_UP) ||
        (currentDirection == SNAKE_DIRECTION_LEFT && requestedDirection == SNAKE_DIRECTION_RIGHT) ||
        (currentDirection == SNAKE_DIRECTION_RIGHT && requestedDirection == SNAKE_DIRECTION_LEFT);
}

static bool IsInsideMap(const SnakeGameCore* game, SnakeGridPosition position)
{
    /*
        地图坐标从 0 开始，所以合法范围是：
        x 在 [0, mapColumns - 1]，y 在 [0, mapRows - 1]。
    */
    return position.x >= 0 &&
        position.y >= 0 &&
        position.x < game->mapColumns &&
        position.y < game->mapRows;
}

static int CalculateMoveIntervalMs(int level)
{
    /*
        速度曲线只依赖等级，是一个非常适合单独封装的小规则：
        - 1 级移动间隔为 180ms；
        - 每升 1 级减少 10ms，看起来就是蛇越来越快；
        - 最低锁在 80ms，避免等级很高时快到无法操作。
        把它写成纯计算函数后，测试不需要推进真实时间，也能直接验证速度。
    */
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
    /*
        这里计算“当前等级应该维持多少个食物”。
        规则是初始 5 个，每 2 级增加 1 个，上限 12 个。
        注意这是目标数量，不一定等于数组容量；真正生成时还要检查地图是否有空格。
    */
    int targetCount = SNAKE_GAME_INITIAL_FOOD_COUNT + level / SNAKE_GAME_FOOD_GROWTH_LEVEL_INTERVAL;

    if (targetCount > SNAKE_GAME_MAX_FOOD_COUNT)
    {
        targetCount = SNAKE_GAME_MAX_FOOD_COUNT;
    }

    return targetCount;
}

static void UpdateLevelSpeedAndFoodTarget(SnakeGameCore* game)
{
    /*
        等级、速度、目标食物数都由“吃过多少食物”推导出来。
        集中在一个函数里同步，可以避免以后只更新了分数、忘了更新速度或食物数量。
        这也是游戏状态常见的写法：原始事件是“吃了一个食物”，其他派生状态统一刷新。
    */
    game->level = game->foodsEaten / SNAKE_GAME_FOODS_PER_LEVEL + 1;
    game->moveIntervalMs = CalculateMoveIntervalMs(game->level);
    game->targetFoodCount = CalculateTargetFoodCount(game->level);
}

static unsigned int NextRandomValue(SnakeGameCore* game)
{
    /*
        使用一个简单的线性同余伪随机数生成器，而不是直接调用 rand()。
        好处是随机状态保存在 SnakeGameCore 内部：
        - 正式游戏可以用时间种子，每局不同；
        - 测试可以用固定种子，每次生成同一批食物；
        - 不依赖 C 运行库的全局 rand 状态，多个测试互不影响。
    */
    game->randomState = game->randomState * 1664525u + 1013904223u;
    return game->randomState;
}

static bool IsPositionOnSnake(const SnakeGameCore* game, SnakeGridPosition position)
{
    if (game == NULL || game->snakeBody == NULL)
    {
        return false;
    }

    /* 从哨兵节点的 next 开始遍历真实蛇身节点，遇到哨兵说明绕回起点。 */
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
    /*
        食物只能出现在真正的空格子上，判断顺序很明确：
        1. 必须在地图范围内；
        2. 不能压在蛇身链表的任何一节上；
        3. 不能和已经存在的其他食物重叠。
        这样玩家看到的每个苹果都是一个独立可吃的目标格。
    */
    return IsInsideMap(game, position) &&
        !IsPositionOnSnake(game, position) &&
        !IsPositionOnFood(game, position);
}

static int CountEmptyFoodCells(const SnakeGameCore* game)
{
    /* 先扫完整张地图，数出当前还有多少个合法空格可以放食物。 */
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
    /* 再按同样的扫描顺序找到第 targetIndex 个空格，和 CountEmptyFoodCells 配套使用。 */
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

    /*
        生成食物不采用“随机一个坐标，非法就重试”的方式。
        当蛇很长、地图很满时，重试法可能尝试很多次还失败；这里先统计所有合法空格，
        再用随机数选择第 N 个合法空格，既稳定又不会出现无限重试。
    */
    const int emptyCellCount = CountEmptyFoodCells(game);
    if (emptyCellCount <= 0)
    {
        return false;
    }

    /* 随机数只决定“选第几个空格”，真正坐标仍由空格扫描函数保证合法。 */
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
    /*
        每次初始化或吃掉食物后，都把食物补到当前等级要求的目标数量。
        如果地图空格不足，AddRandomFood 会失败并跳出循环；这比硬塞食物更安全，
        也能自然处理后期蛇身几乎占满地图的情况。
    */
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

    /*
        食物数组的顺序没有玩法含义，玩家只关心地图上有哪些坐标有苹果。
        删除某个食物后，把后面的元素整体前移，保证有效食物始终紧凑地放在
        foods[0..foodCount) 中。这样查询、去重和补食物都只需要遍历有效范围。
    */
    for (int i = foodIndex; i < game->foodCount - 1; ++i)
    {
        game->foods[i] = game->foods[i + 1];
    }

    --game->foodCount;
}
static bool DoesNextHeadHitSnakeBody(const SnakeGameCore* game, SnakeGridPosition nextHead)
{
    /*
        先拿到哨兵节点和旧尾节点。双向循环链表的结构正好适合这里：
        snakeBody 是哨兵，snakeBody->next 是蛇头，snakeBody->prev 是蛇尾。
    */
    const DCListNode* sentinel = game->snakeBody;
    const DCListNode* oldTail = sentinel->prev;

    /*
        自撞要在真正改链表前判断。普通移动本帧会执行“头插新节点、尾删旧节点”，
        所以旧尾所在格子马上会空出来，蛇头走到旧尾位置不能算撞到自己。
    */
    for (const DCListNode* segment = sentinel->next; segment != sentinel; segment = segment->next)
    {
        /* 遍历到旧尾时直接跳过，因为它会在本帧尾删时离开地图格子。 */
        if (segment == oldTail)
        {
            continue;
        }

        /* 只要新蛇头与仍会保留的身体节点重合，就说明发生了自撞。 */
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
        链表按“蛇头到蛇尾”的顺序保存蛇身：

            L -> 蛇头 -> 身体 -> 蛇尾 -> L

        初始化时按这个顺序尾插三节身体，这样 L->next 会立刻指向蛇头。
        这就是链表结构和贪吃蛇规则之间最关键的对应关系。
    */
    /* 先确定蛇头位置，后续身体节点都以蛇头为基准向左排列。 */
    const SnakeGridPosition head = GetInitialHeadPosition();

    /* 循环创建初始三节身体：i 为 0 时是蛇头，i 越大越靠近蛇尾。 */
    for (int i = 0; i < SNAKE_GAME_INITIAL_LENGTH; ++i)
    {
        /* 初始方向向右，所以身体要放在蛇头左侧，x 每次减 1，y 保持不变。 */
        SnakeGridPosition segment = {
            head.x - i,
            head.y
        };

        /* 用尾插保持“蛇头 -> 身体 -> 蛇尾”的链表顺序。 */
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

    /* 地图尺寸是核心规则中的格子数量；界面层以后再把格子换算成像素矩形。 */
    game->mapColumns = SNAKE_GAME_GRID_COLUMNS;
    game->mapRows = SNAKE_GAME_GRID_ROWS;

    /* 先把玩法计数全部放回 1 级开局状态，避免后续初始化步骤读到旧数据。 */
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

    /* 蛇身仍然由双向循环链表拥有；食物和计分逻辑只是在链表移动结果上叠加规则。 */
    game->snakeBody = DCListInit();
    BuildInitialSnake(game->snakeBody);
    game->snakeLength = DCListSize(game->snakeBody);

    UpdateLevelSpeedAndFoodTarget(game);
    FillFoodsToTarget(game);

    game->status = SNAKE_GAME_STATUS_RUNNING;

    return game->snakeLength == SNAKE_GAME_INITIAL_LENGTH &&
        game->foodCount == SNAKE_GAME_INITIAL_FOOD_COUNT;
}
void SnakeGameDestroy(SnakeGameCore* game)
{
    if (game == NULL)
    {
        return;
    }

    /*
        通过游戏核心统一销毁蛇身，可以让所有权更清楚：谁调用
        SnakeGameInit，谁就调用 SnakeGameDestroy；界面层不直接释放链表节点。
        销毁后把指针置空，可以让第二次销毁安全返回，这对单元测试和后续
        “重新开始一局”的流程都更稳妥。
    */
    /* 先释放整条蛇身链表，包括哨兵节点和所有真实身体节点。 */
    DCListDestroy(game->snakeBody);

    /* 销毁后立刻把指针置空，避免后续误用已经释放的链表地址。 */
    game->snakeBody = NULL;

    /* 蛇长清零，状态回到 READY，表示这个核心对象已经不再持有一局正在运行的游戏。 */
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

    /* 死亡后即使界面层继续调用 Step，也保持原地不动，避免死后蛇身继续变化。 */
    if (game->status == SNAKE_GAME_STATUS_DEAD)
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
        蛇的移动仍然完全通过双向循环链表表达：
        - 每一帧都在链表头部插入一个“新蛇头”节点；
        - 如果没吃到食物，就删除链表尾部的旧尾巴，形成“头插尾删”，长度不变；
        - 如果吃到食物，就故意不删尾巴，新增的头节点留下来，蛇身自然增长 1 节。
        这正是链表适合贪吃蛇的地方：移动只改头尾指针，不需要搬动中间每一节身体。
    */
    DCListPushFront(game->snakeBody, nextHead);

    if (ateFood)
    {
        /* 新蛇头落在食物格上，先把这个苹果从数组中移除。 */
        RemoveFoodAt(game, foodIndex);

        /* 吃到红苹果后，本局蛇的语义颜色永久切换为红色；界面层会据此换调色板。 */
        game->snakeColorMode = SNAKE_COLOR_MODE_RED;

        /* 本项目规则中，一个食物就是 1 分，同时累计本局吃到的食物数。 */
        ++game->score;
        ++game->foodsEaten;
        UpdateLevelSpeedAndFoodTarget(game);

        game->snakeLength = DCListSize(game->snakeBody);
        /* 吃掉一个后，根据升级后的目标食物数量重新补齐地图上的苹果。 */
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

    /*
        如果玩家请求 180 度反向，新蛇头会直接撞进当前第二节身体。
        经典贪吃蛇的做法是忽略这次输入，而不是立刻判死。
    */
    if (IsOppositeDirection(game->direction, requestedDirection))
    {
        return true;
    }

    /* 只有非反向输入才真正更新方向，下一次 Step 会按这个方向计算新蛇头。 */
    game->direction = requestedDirection;
    return true;
}

bool SnakeGameGetSnakeHead(const SnakeGameCore* game, SnakeGridPosition* outPosition)
{
    /* 蛇头就是链表顺序中的第 0 节，复用通用的按下标读取接口即可。 */
    return SnakeGameGetSnakeSegment(game, 0, outPosition);
}

bool SnakeGameGetSnakeTail(const SnakeGameCore* game, SnakeGridPosition* outPosition)
{
    /* 先检查输入和链表是否有效；只要没有真实蛇身节点，就不能读取蛇尾。 */
    if (game == NULL || outPosition == NULL || game->snakeBody == NULL || game->snakeLength <= 0)
    {
        return false;
    }

    /* 双向循环链表的哨兵节点 prev 永远指向最后一个真实节点，也就是蛇尾。 */
    const DCListNode* tail = game->snakeBody->prev;

    /* 如果 prev 又指回哨兵，说明链表是空的，没有可以返回的蛇尾坐标。 */
    if (tail == game->snakeBody)
    {
        return false;
    }

    /* 把蛇尾节点里的网格坐标复制给调用方，调用方不需要接触链表节点本身。 */
    *outPosition = tail->data;
    return true;
}

bool SnakeGameGetSnakeSegment(const SnakeGameCore* game, int index, SnakeGridPosition* outPosition)
{
    /* 参数或链表为空时直接失败，避免下面访问空指针。 */
    if (game == NULL || outPosition == NULL || game->snakeBody == NULL)
    {
        return false;
    }

    /* 下标必须落在 [0, snakeLength - 1]，其中 0 是蛇头，最后一个是蛇尾。 */
    if (index < 0 || index >= game->snakeLength)
    {
        return false;
    }

    /* 从链表头部按顺序取第 index 个真实节点，再只把坐标数据交给调用方。 */
    const DCListNode* segment = DCListGetConstElem(game->snakeBody, index);
    *outPosition = segment->data;
    return true;
}
