#include "gtest/gtest.h"

#include "game_core.h"

namespace
{
void ExpectPosition(const SnakeGridPosition& position, int expectedX, int expectedY)
{
    // 坐标断言拆成 x 和 y 两个维度，失败时能直接看到是哪一个方向错了。
    EXPECT_EQ(expectedX, position.x);
    EXPECT_EQ(expectedY, position.y);
}

void ExpectSnakeSegments(
    const SnakeGameCore* game,
    const SnakeGridPosition* expectedSegments,
    int expectedSegmentCount)
{
    // 先验证蛇长，确认普通移动没有错误地增长或丢失身体节点。
    ASSERT_EQ(expectedSegmentCount, SnakeGameGetSnakeLength(game));

    // 按“蛇头到蛇尾”的顺序逐节读取，确保链表头插尾删后的整体顺序正确。
    for (int i = 0; i < expectedSegmentCount; ++i)
    {
        SnakeGridPosition actualSegment{};
        ASSERT_TRUE(SnakeGameGetSnakeSegment(game, i, &actualSegment));
        ExpectPosition(actualSegment, expectedSegments[i].x, expectedSegments[i].y);
    }
}

void ReplaceSnake(
    SnakeGameCore* game,
    const SnakeGridPosition* segments,
    int segmentCount,
    SnakeDirection direction)
{
    // 先销毁初始化时自动生成的蛇身，避免测试数据和默认三节蛇混在一起。
    DCListDestroy(game->snakeBody);

    // 重新创建一个空的双向循环链表，后面按测试需要放入指定蛇形。
    game->snakeBody = DCListInit();

    // 按“蛇头到蛇尾”的顺序插入节点；第 0 个坐标会成为蛇头，最后一个坐标会成为蛇尾。
    for (int i = 0; i < segmentCount; ++i)
    {
        DCListPushBack(game->snakeBody, segments[i]);
    }

    // 手动构造蛇形后同步核心状态，让长度、方向和运行状态都与链表一致。
    game->snakeLength = DCListSize(game->snakeBody);
    game->direction = direction;
    game->status = SNAKE_GAME_STATUS_RUNNING;
}
}

TEST(GameCorePhase2, InitializesMapDirectionAndInitialSnake)
{
    SnakeGameCore game{};

    // 初始化会创建 30 x 20 地图、长度为 3 的蛇，并让蛇头位于地图中间。
    ASSERT_TRUE(SnakeGameInit(&game));

    // 验证游戏核心的基础状态，保证后续移动测试建立在正确的初始条件上。
    EXPECT_EQ(SNAKE_GAME_GRID_COLUMNS, SnakeGameGetMapColumns(&game));
    EXPECT_EQ(SNAKE_GAME_GRID_ROWS, SnakeGameGetMapRows(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH, SnakeGameGetSnakeLength(&game));
    EXPECT_EQ(SNAKE_DIRECTION_RIGHT, SnakeGameGetDirection(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));

    SnakeGridPosition segment{};
    // 第 0 节是蛇头；初始方向向右，所以蛇头在地图中间 (15, 10)。
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 0, &segment));
    ExpectPosition(segment, 15, 10);

    // 第 1 节和第 2 节依次排在蛇头左侧，形成“向右前进”的初始蛇形。
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 1, &segment));
    ExpectPosition(segment, 14, 10);

    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 2, &segment));
    ExpectPosition(segment, 13, 10);

    SnakeGridPosition head{};
    SnakeGridPosition tail{};
    // 额外验证专门的蛇头、蛇尾读取接口，确保它们和按下标读取的结果一致。
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ASSERT_TRUE(SnakeGameGetSnakeTail(&game, &tail));
    ExpectPosition(head, 15, 10);
    ExpectPosition(tail, 13, 10);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase2, StepPushesNewHeadAndRemovesOldTail)
{
    SnakeGameCore game{};
    // 使用默认初始蛇形：蛇头 (15,10)，第二节 (14,10)，尾巴 (13,10)。
    ASSERT_TRUE(SnakeGameInit(&game));

    SnakeGridPosition oldHead{};
    SnakeGridPosition oldSecond{};
    // 先记录旧蛇头和旧第二节，用来验证移动后的“头插尾删”效果。
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 0, &oldHead));
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 1, &oldSecond));

    // 推进一步后，核心会在前方插入新蛇头，并删除旧蛇尾。
    ASSERT_TRUE(SnakeGameStep(&game));

    // 普通移动没有吃食物，所以蛇长必须保持不变。
    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH, SnakeGameGetSnakeLength(&game));

    SnakeGridPosition segment{};
    // 新蛇头应该在旧蛇头右侧一格。
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 0, &segment));
    ExpectPosition(segment, oldHead.x + 1, oldHead.y);

    // 旧蛇头会变成新的第二节，说明链表头插成功。
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 1, &segment));
    ExpectPosition(segment, oldHead.x, oldHead.y);

    // 旧第二节会变成新的尾巴，说明旧尾巴已经被删除。
    ASSERT_TRUE(SnakeGameGetSnakeTail(&game, &segment));
    ExpectPosition(segment, oldSecond.x, oldSecond.y);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase2, DestroyIsSafeToCallTwice)
{
    SnakeGameCore game{};
    // 先创建一局正常游戏，确保 Destroy 有真实链表资源可以释放。
    ASSERT_TRUE(SnakeGameInit(&game));

    // 第一次销毁应释放蛇身链表，并把核心状态恢复为 READY。
    SnakeGameDestroy(&game);
    EXPECT_EQ(0, SnakeGameGetSnakeLength(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_READY, SnakeGameGetStatus(&game));

    // 第二次销毁用于验证接口的容错性：重复释放不应崩溃，也不应恢复出蛇身。
    SnakeGameDestroy(&game);
    EXPECT_EQ(0, SnakeGameGetSnakeLength(&game));
}

TEST(GameCorePhase4, RequestedDirectionsMoveHeadOneCellInThatDirection)
{
    struct DirectionCase
    {
        SnakeDirection startingDirection;
        SnakeDirection requestedDirection;
        SnakeGridPosition expectedSegments[3];
    };

    const DirectionCase cases[] = {
        // 保持向右时，验证“新蛇头 + 旧蛇头变第二节 + 旧第二节变蛇尾”。
        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_RIGHT,
            { { 16, 10 }, { 15, 10 }, { 14, 10 } }
        },
        // 当前方向向右，请求转向上方后，新蛇头 y 减 1，其余身体按链表顺序后移。
        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_UP,
            { { 15, 9 }, { 15, 10 }, { 14, 10 } }
        },
        // 当前方向向右，请求转向下方后，新蛇头 y 加 1，其余身体按链表顺序后移。
        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_DOWN,
            { { 15, 11 }, { 15, 10 }, { 14, 10 } }
        },
        // 从向上改为向左是合法转向，竖直蛇形移动后旧蛇头和旧第二节依次后移。
        {
            SNAKE_DIRECTION_UP,
            SNAKE_DIRECTION_LEFT,
            { { 14, 10 }, { 15, 10 }, { 15, 11 } }
        },
    };

    for (const DirectionCase& testCase : cases)
    {
        SnakeGameCore game{};
        // 每个方向用例都从一局干净的游戏开始，避免上一个用例影响链表状态。
        ASSERT_TRUE(SnakeGameInit(&game));

        if (testCase.requestedDirection == SNAKE_DIRECTION_LEFT)
        {
            // 默认蛇形向左会撞到自己的第二节，所以这里改成竖直蛇形来单独测试左移。
            const SnakeGridPosition verticalSnake[] = {
                { 15, 10 },
                { 15, 11 },
                { 15, 12 },
            };
            ReplaceSnake(&game, verticalSnake, 3, testCase.startingDirection);
        }
        else
        {
            // 其他用例使用默认初始蛇形，只需要指定当前朝向即可。
            game.direction = testCase.startingDirection;
        }

        // 先提交方向请求，再推进一格，验证方向请求确实影响下一步移动。
        ASSERT_TRUE(SnakeGameRequestDirection(&game, testCase.requestedDirection));
        ASSERT_TRUE(SnakeGameStep(&game));

        // 不只检查蛇头，还逐节检查整条蛇，确认头插尾删后的链表顺序正确。
        ExpectSnakeSegments(&game, testCase.expectedSegments, SNAKE_GAME_INITIAL_LENGTH);

        SnakeGameDestroy(&game);
    }
}

TEST(GameCorePhase4, DirectionRequestCoversAllCurrentAndRequestedDirectionCombinations)
{
    struct DirectionRequestCase
    {
        // startingDirection 表示请求前的当前方向，也就是核心里原本保存的方向。
        SnakeDirection startingDirection;

        // requestedDirection 表示本次传入 SnakeGameRequestDirection 的方向请求。
        SnakeDirection requestedDirection;

        // expectedDirection 表示请求处理完成后核心里应该保存的方向。
        // 如果是 180 度反向，就应该保持 startingDirection；否则应该变成 requestedDirection。
        SnakeDirection expectedDirection;
    };

    const DirectionRequestCase cases[] = {
        // 当前方向为上：请求下方是反向，需要被忽略；其他方向都应该被接受。
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT },
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT },

        // 当前方向为右：请求左方是反向，需要被忽略；其他方向都应该被接受。
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT },
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_RIGHT },

        // 当前方向为下：请求上方是反向，需要被忽略；其他方向都应该被接受。
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT },
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT },

        // 当前方向为左：请求右方是反向，需要被忽略；其他方向都应该被接受。
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_LEFT },
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT },
    };

    for (const DirectionRequestCase& testCase : cases)
    {
        SnakeGameCore game{};
        ASSERT_TRUE(SnakeGameInit(&game));

        // 这个测试只关心“方向请求处理结果”，所以直接设置当前方向，不推进蛇身。
        game.direction = testCase.startingDirection;

        // 提交方向请求后，核心层会根据禁止 180 度反向规则决定是否更新 direction。
        ASSERT_TRUE(SnakeGameRequestDirection(&game, testCase.requestedDirection));

        // 验证 16 种 currentDirection/requestedDirection 组合处理后的最终方向都符合预期。
        EXPECT_EQ(testCase.expectedDirection, SnakeGameGetDirection(&game));

        // 完整移动后的蛇身坐标由其他测试负责，这里只释放本用例创建的链表资源。
        SnakeGameDestroy(&game);
    }
}
TEST(GameCorePhase4, DirectReverseDirectionRequestIsIgnored)
{
    struct ReverseCase
    {
        // startingDirection 表示蛇当前真正的移动方向，也是反向请求被忽略后应保持的方向。
        SnakeDirection startingDirection;

        // requestedDirection 表示玩家本次请求的方向，这里专门填写与当前方向相反的方向。
        SnakeDirection requestedDirection;

        // initialSegments 按“蛇头到蛇尾”的顺序描述测试开始前的蛇身。
        // 这些蛇形都与 startingDirection 匹配，保证如果继续按当前方向移动，不会立刻自撞。
        SnakeGridPosition initialSegments[3];

        // expectedSegmentsAfterStep 表示反向请求被忽略后，继续按原方向 Step 一次的完整蛇身。
        // 这里不只检查蛇头，也检查旧蛇头是否变成第二节、旧第二节是否变成蛇尾。
        SnakeGridPosition expectedSegmentsAfterStep[3];
    };

    const ReverseCase cases[] = {
        // 当前向上，请求向下是 180 度反向；请求应被忽略，蛇继续向上移动一格。
        {
            SNAKE_DIRECTION_UP,
            SNAKE_DIRECTION_DOWN,
            { { 15, 10 }, { 15, 11 }, { 15, 12 } },
            { { 15, 9 }, { 15, 10 }, { 15, 11 } }
        },
        // 当前向下，请求向上是 180 度反向；请求应被忽略，蛇继续向下移动一格。
        {
            SNAKE_DIRECTION_DOWN,
            SNAKE_DIRECTION_UP,
            { { 15, 10 }, { 15, 9 }, { 15, 8 } },
            { { 15, 11 }, { 15, 10 }, { 15, 9 } }
        },
        // 当前向左，请求向右是 180 度反向；请求应被忽略，蛇继续向左移动一格。
        {
            SNAKE_DIRECTION_LEFT,
            SNAKE_DIRECTION_RIGHT,
            { { 15, 10 }, { 16, 10 }, { 17, 10 } },
            { { 14, 10 }, { 15, 10 }, { 16, 10 } }
        },
        // 当前向右，请求向左是 180 度反向；请求应被忽略，蛇继续向右移动一格。
        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_LEFT,
            { { 15, 10 }, { 14, 10 }, { 13, 10 } },
            { { 16, 10 }, { 15, 10 }, { 14, 10 } }
        },
    };

    for (const ReverseCase& testCase : cases)
    {
        SnakeGameCore game{};
        ASSERT_TRUE(SnakeGameInit(&game));

        // 每个反向组合都要先构造一条与当前方向匹配的蛇，避免测试被无关自撞干扰。
        ReplaceSnake(&game, testCase.initialSegments, SNAKE_GAME_INITIAL_LENGTH, testCase.startingDirection);

        // 提交反方向请求。核心应接受调用本身，但忽略这个非法转向。
        ASSERT_TRUE(SnakeGameRequestDirection(&game, testCase.requestedDirection));

        // 方向必须保持为原来的 startingDirection，说明 IsOppositeDirection 对该组合判断生效。
        EXPECT_EQ(testCase.startingDirection, SnakeGameGetDirection(&game));

        // 再推进一步。如果反向请求真的被忽略，蛇会继续沿原方向执行“头插尾删”。
        ASSERT_TRUE(SnakeGameStep(&game));

        // 检查完整蛇身，确认新蛇头、旧蛇头、旧第二节都处在预期位置。
        ExpectSnakeSegments(&game, testCase.expectedSegmentsAfterStep, SNAKE_GAME_INITIAL_LENGTH);

        SnakeGameDestroy(&game);
    }
}

TEST(GameCorePhase4, NonReverseTurnRequestsChangeDirection)
{
    SnakeGameCore game{};
    // 默认方向向右，用来验证非反向的连续转向请求都会被接受。
    ASSERT_TRUE(SnakeGameInit(&game));

    // 向右改为向上不是反向，应该立即更新方向。
    ASSERT_TRUE(SnakeGameRequestDirection(&game, SNAKE_DIRECTION_UP));
    EXPECT_EQ(SNAKE_DIRECTION_UP, SnakeGameGetDirection(&game));

    // 向上改为向右不是反向，也应该立即更新方向。
    ASSERT_TRUE(SnakeGameRequestDirection(&game, SNAKE_DIRECTION_RIGHT));
    EXPECT_EQ(SNAKE_DIRECTION_RIGHT, SnakeGameGetDirection(&game));

    // 向右改为向下不是反向，说明核心只拒绝 180 度掉头。
    ASSERT_TRUE(SnakeGameRequestDirection(&game, SNAKE_DIRECTION_DOWN));
    EXPECT_EQ(SNAKE_DIRECTION_DOWN, SnakeGameGetDirection(&game));

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase4, WallCollisionKillsSnakeOnAllFourEdges)
{
    struct WallCase
    {
        SnakeGridPosition segments[3];
        SnakeDirection direction;
    };

    const WallCase cases[] = {
        // 蛇头在最左列再向左走，会越过 x = 0 的边界。
        { { { 0, 5 }, { 1, 5 }, { 2, 5 } }, SNAKE_DIRECTION_LEFT },
        // 蛇头在最右列再向右走，会越过 x = 29 的边界。
        { { { 29, 5 }, { 28, 5 }, { 27, 5 } }, SNAKE_DIRECTION_RIGHT },
        // 蛇头在最上行再向上走，会越过 y = 0 的边界。
        { { { 5, 0 }, { 5, 1 }, { 5, 2 } }, SNAKE_DIRECTION_UP },
        // 蛇头在最下行再向下走，会越过 y = 19 的边界。
        { { { 5, 19 }, { 5, 18 }, { 5, 17 } }, SNAKE_DIRECTION_DOWN },
    };

    for (const WallCase& testCase : cases)
    {
        SnakeGameCore game{};
        ASSERT_TRUE(SnakeGameInit(&game));
        // 构造蛇头贴着边界的蛇形，让下一步必定撞墙。
        ReplaceSnake(&game, testCase.segments, 3, testCase.direction);

        // 撞墙后 Step 返回成功表示“规则处理完成”，状态应变为 DEAD。
        ASSERT_TRUE(SnakeGameStep(&game));
        EXPECT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

        SnakeGameDestroy(&game);
    }
}

TEST(GameCorePhase4, DeadSnakeDoesNotMoveOnLaterSteps)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));

    // 蛇头放在最右列，并让方向向右，第一次 Step 会触发撞墙死亡。
    const SnakeGridPosition segments[] = {
        { 29, 5 },
        { 28, 5 },
        { 27, 5 },
    };
    ReplaceSnake(&game, segments, 3, SNAKE_DIRECTION_RIGHT);

    // 先制造死亡状态，确认测试对象已经进入 DEAD。
    ASSERT_TRUE(SnakeGameStep(&game));
    ASSERT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

    SnakeGridPosition headAfterDeath{};
    // 记录死亡时的蛇头坐标和蛇长，用来验证死亡后再次 Step 不会改变蛇身。
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfterDeath));
    const int lengthAfterDeath = SnakeGameGetSnakeLength(&game);

    // 死亡后继续调用 Step，核心应该直接返回，不再执行移动。
    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition headAfterLaterStep{};
    // 再次读取蛇头和蛇长，确认它们与死亡瞬间保持一致。
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfterLaterStep));
    ExpectPosition(headAfterLaterStep, headAfterDeath.x, headAfterDeath.y);
    EXPECT_EQ(lengthAfterDeath, SnakeGameGetSnakeLength(&game));

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase4, HittingNonTailBodySegmentKillsSnake)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));

    // 构造一个弯折蛇形：蛇头向上走会撞到第二节身体，而不是撞到旧尾。
    const SnakeGridPosition segments[] = {
        { 5, 5 },
        { 5, 4 },
        { 4, 4 },
        { 4, 5 },
    };
    ReplaceSnake(&game, segments, 4, SNAKE_DIRECTION_UP);

    // 下一步新蛇头目标是 (5,4)，这个位置仍是身体节点，所以应判定自撞死亡。
    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase4, MovingIntoOldTailCellIsAllowedBecauseTailMovesAway)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));

    // 这个蛇形的旧尾在 (4,5)；蛇头向左走也会到 (4,5)。
    const SnakeGridPosition segments[] = {
        { 5, 5 },
        { 5, 4 },
        { 4, 4 },
        { 4, 5 },
    };
    ReplaceSnake(&game, segments, 4, SNAKE_DIRECTION_LEFT);

    // 普通移动会删除旧尾，所以新蛇头走进旧尾格子应该是合法移动。
    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));
    EXPECT_EQ(4, SnakeGameGetSnakeLength(&game));

    SnakeGridPosition head{};
    // 移动后蛇头应该正好在旧尾所在的 (4,5)，用来证明“旧尾不算自撞”。
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ExpectPosition(head, 4, 5);

    SnakeGameDestroy(&game);
}
