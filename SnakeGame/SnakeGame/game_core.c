#include "game_core.h"

#include <stddef.h>

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
    if (game == NULL)
    {
        return false;
    }

    /* 初始化地图尺寸，这些值是核心规则使用的网格大小，不是窗口像素大小。 */
    game->mapColumns = SNAKE_GAME_GRID_COLUMNS;
    game->mapRows = SNAKE_GAME_GRID_ROWS;

    /* 先把普通字段放到明确的起点状态，避免使用未初始化数据。 */
    game->snakeLength = 0;
    game->direction = SNAKE_DIRECTION_RIGHT;
    game->status = SNAKE_GAME_STATUS_READY;

    /* 创建蛇身链表的哨兵节点，后面所有蛇身节点都挂在这个链表里。 */
    game->snakeBody = DCListInit();

    /* 按规格创建初始三节蛇身，并把蛇长同步为链表中的真实节点数量。 */
    BuildInitialSnake(game->snakeBody);
    game->snakeLength = DCListSize(game->snakeBody);

    /* 初始蛇身创建成功后进入运行状态，界面层就可以开始按计时器推进。 */
    game->status = SNAKE_GAME_STATUS_RUNNING;

    /* 返回值用来告诉调用方：初始链表长度是否和规格中的初始蛇长一致。 */
    return game->snakeLength == SNAKE_GAME_INITIAL_LENGTH;
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
}

bool SnakeGameStep(SnakeGameCore* game)
{
    if (game == NULL || game->snakeBody == NULL || game->snakeLength <= 0)
    {
        return false;
    }

    /* 死亡后再次推进时保持原地不动，避免界面层继续调用 Step 时蛇身还在移动。 */
    if (game->status == SNAKE_GAME_STATUS_DEAD)
    {
        return true;
    }

    SnakeGridPosition currentHead;
    if (!SnakeGameGetSnakeHead(game, &currentHead))
    {
        return false;
    }

    /* 先根据当前方向算出下一格蛇头坐标，后面的撞墙和自撞都检查这个目标格。 */
    const SnakeGridPosition nextHead = GetNextHeadPosition(currentHead, game->direction);

    /* 如果下一格已经超出地图边界，蛇不再移动，直接进入死亡状态。 */
    if (!IsInsideMap(game, nextHead))
    {
        game->status = SNAKE_GAME_STATUS_DEAD;
        return true;
    }

    /* 如果下一格撞到仍会保留的身体节点，蛇也不再移动，直接进入死亡状态。 */
    if (DoesNextHeadHitSnakeBody(game, nextHead))
    {
        game->status = SNAKE_GAME_STATUS_DEAD;
        return true;
    }

    /*
        普通移动对应链表的“头插尾删”：先在头部插入新蛇头，再删除旧蛇尾。
        这样不用搬动中间节点的数据，中间身体节点只是因为链表顺序向后顺延，
        看起来就像整条蛇前进了一格。
    */
    DCListPushFront(game->snakeBody, nextHead);
    DCListPopBack(game->snakeBody);

    /* 链表节点数量是蛇长的来源，移动后重新同步一次，保证状态和链表一致。 */
    game->snakeLength = DCListSize(game->snakeBody);
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
