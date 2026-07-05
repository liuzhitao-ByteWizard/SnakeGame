#ifndef SNAKE_GAME_CORE_H
#define SNAKE_GAME_CORE_H

#include "DCList.h"
#include "grid_position.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    游戏核心使用固定的网格尺寸：30 列、20 行。
    这里定义的是“逻辑地图”的大小，不是窗口像素大小。界面层会根据
    单元格像素尺寸把这些网格坐标换算成屏幕上的矩形。
*/
#define SNAKE_GAME_GRID_COLUMNS 30
#define SNAKE_GAME_GRID_ROWS 20

/*
    第一版贪吃蛇出生时有 3 节身体：蛇头 + 两节身体。
    初始蛇头位于地图中间，身体沿着初始方向的反方向排列。
*/
#define SNAKE_GAME_INITIAL_LENGTH 3

/*
    蛇的移动方向。

    枚举值只表示游戏核心能理解的方向，不直接绑定键盘按键。
    后续界面层会把方向键或 WASD 转换成这些枚举值，再交给核心层处理。
*/
typedef enum SnakeDirection
{
    SNAKE_DIRECTION_UP = 0,
    SNAKE_DIRECTION_RIGHT, //1
    SNAKE_DIRECTION_DOWN, //2
    SNAKE_DIRECTION_LEFT //3
} SnakeDirection;

/*
    游戏核心状态。

    Phase 2 只真正使用 READY 和 RUNNING；DEAD 会在后续碰撞阶段接入。
    先把状态枚举放在核心接口里，可以让界面层以后只根据状态绘制开始、
    游戏中、暂停或结束界面，而不用猜核心内部发生了什么。
*/
typedef enum SnakeGameStatus
{
    SNAKE_GAME_STATUS_READY = 0,
    SNAKE_GAME_STATUS_RUNNING,
    SNAKE_GAME_STATUS_DEAD
} SnakeGameStatus;

/*
    贪吃蛇游戏核心的完整数据。

    这个结构体只保存规则层需要的数据，不包含 raylib 类型，也不保存像素坐标。
    snakeBody 是带哨兵头节点的双向循环链表：
      - snakeBody->next 指向蛇头节点。
      - snakeBody->prev 指向蛇尾节点。

    snakeLength 单独保存蛇身长度，方便界面层和测试直接读取；每次移动后会和
    链表节点数量保持一致。
*/
typedef struct SnakeGameCore
{
    int mapColumns;              /* 地图列数，当前固定为 30。 */
    int mapRows;                 /* 地图行数，当前固定为 20。 */
    int snakeLength;             /* 当前蛇身长度，也就是链表中的真实节点数量。 */
    SnakeDirection direction;    /* 当前移动方向，Phase 2 初始为向右。 */
    SnakeGameStatus status;      /* 当前游戏状态，供界面层判断该显示哪类画面。 */
    DCListNode* snakeBody;       /* 蛇身链表；哨兵节点本身不代表蛇身。 */
} SnakeGameCore;

/*
    初始化游戏核心。

    调用成功后会创建蛇身链表，并生成初始蛇身：
      - 地图尺寸为 30 x 20。
      - 蛇长为 3。
      - 蛇头在地图中间。
      - 初始方向向右。

    返回 true 表示初始化成功；传入 NULL 时返回 false。
*/
bool SnakeGameInit(SnakeGameCore* game);

/*
    销毁游戏核心持有的链表资源。

    这个函数会释放蛇身链表，并把 game 内部指针置空。调用者不应该直接释放
    snakeBody，因为链表资源属于游戏核心统一管理。
*/
void SnakeGameDestroy(SnakeGameCore* game);

/*
    推进一帧核心移动逻辑。

    Phase 2 的单步推进只做普通移动：根据当前方向计算新蛇头，向链表头部插入
    新节点，再删除链表尾部节点，所以蛇长保持不变。食物、增长、转向和碰撞
    会在后续 Phase 中继续补充。
*/
bool SnakeGameStep(SnakeGameCore* game);

/* 以下函数提供只读查询接口，方便界面层绘制，也方便单元测试检查核心状态。 */
int SnakeGameGetMapColumns(const SnakeGameCore* game);
int SnakeGameGetMapRows(const SnakeGameCore* game);
int SnakeGameGetSnakeLength(const SnakeGameCore* game);
SnakeDirection SnakeGameGetDirection(const SnakeGameCore* game);
SnakeGameStatus SnakeGameGetStatus(const SnakeGameCore* game);
/* 请求改变移动方向；具体的禁止反向规则由 game_core.c 在执行语句前说明。 */
bool SnakeGameRequestDirection(SnakeGameCore* game, SnakeDirection requestedDirection);

/*
    读取蛇头坐标。

    蛇头就是链表中的第 0 个真实节点，也就是 snakeBody->next。
    outPosition 用于带出坐标；参数无效或蛇身为空时返回 false。
*/
bool SnakeGameGetSnakeHead(const SnakeGameCore* game, SnakeGridPosition* outPosition);

/*
    读取蛇尾坐标。
    蛇尾就是链表中的最后一个真实节点，也就是 snakeBody->prev。
    普通移动时被删除的正是这个节点。
*/
bool SnakeGameGetSnakeTail(const SnakeGameCore* game, SnakeGridPosition* outPosition);

/*
    按下标读取蛇身任意一节的位置。
    index 从 0 开始：0 表示蛇头，snakeLength - 1 表示蛇尾。这个接口让测试和
    后续渲染层可以按“从头到尾”的顺序遍历蛇身，而不需要直接操作链表指针。
*/
bool SnakeGameGetSnakeSegment(const SnakeGameCore* game, int index, SnakeGridPosition* outPosition);

#ifdef __cplusplus
}
#endif

#endif