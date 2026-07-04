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

static void BuildInitialSnake(DCListNode* snakeBody)
{
    /*
        链表按“蛇头到蛇尾”的顺序保存蛇身：

            L -> 蛇头 -> 身体 -> 蛇尾 -> L

        初始化时按这个顺序尾插三节身体，这样 L->next 会立刻指向蛇头。
        这就是链表结构和贪吃蛇规则之间最关键的对应关系。
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
    if (game == NULL)
    {
        return false;
    }

    game->mapColumns = SNAKE_GAME_GRID_COLUMNS;
    game->mapRows = SNAKE_GAME_GRID_ROWS;
    game->snakeLength = 0;
    game->direction = SNAKE_DIRECTION_RIGHT;
    game->status = SNAKE_GAME_STATUS_READY;
    game->snakeBody = DCListInit();

    BuildInitialSnake(game->snakeBody);
    game->snakeLength = DCListSize(game->snakeBody);
    game->status = SNAKE_GAME_STATUS_RUNNING;

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
    DCListDestroy(game->snakeBody);
    game->snakeBody = NULL;
    game->snakeLength = 0;
    game->status = SNAKE_GAME_STATUS_READY;
}

bool SnakeGameStep(SnakeGameCore* game)
{
    if (game == NULL || game->snakeBody == NULL || game->snakeLength <= 0)
    {
        return false;
    }

    SnakeGridPosition currentHead;
    if (!SnakeGameGetSnakeHead(game, &currentHead))
    {
        return false;
    }

    /*
        普通移动可以概括为“新增蛇头，删除旧蛇尾”。

        从画面上看，蛇身像是整体向前移动了一格；从链表角度看，中间节点
        实际还留在链表里，只是它们的含义向后顺延：旧蛇头变成第二节，
        旧第二节变成第三节，旧蛇尾被删除。双向链表很适合这个规则，
        因为新增头部和删除尾部都只发生在链表两端。
    */
    const SnakeGridPosition nextHead = GetNextHeadPosition(currentHead, game->direction);
    DCListPushFront(game->snakeBody, nextHead);
    DCListPopBack(game->snakeBody);

    game->snakeLength = DCListSize(game->snakeBody);
    return game->snakeLength == SNAKE_GAME_INITIAL_LENGTH;
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
