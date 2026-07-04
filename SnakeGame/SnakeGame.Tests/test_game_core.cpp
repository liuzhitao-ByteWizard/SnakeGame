#include "gtest/gtest.h"

#include "game_core.h"

namespace
{
void ExpectPosition(const SnakeGridPosition& position, int expectedX, int expectedY)
{
    EXPECT_EQ(expectedX, position.x);
    EXPECT_EQ(expectedY, position.y);
}
}

TEST(GameCorePhase2, InitializesMapDirectionAndInitialSnake)
{
    SnakeGameCore game{};

    ASSERT_TRUE(SnakeGameInit(&game));

    EXPECT_EQ(SNAKE_GAME_GRID_COLUMNS, SnakeGameGetMapColumns(&game));
    EXPECT_EQ(SNAKE_GAME_GRID_ROWS, SnakeGameGetMapRows(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH, SnakeGameGetSnakeLength(&game));
    EXPECT_EQ(SNAKE_DIRECTION_RIGHT, SnakeGameGetDirection(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));

    SnakeGridPosition segment{};
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 0, &segment));
    ExpectPosition(segment, 15, 10);

    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 1, &segment));
    ExpectPosition(segment, 14, 10);

    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 2, &segment));
    ExpectPosition(segment, 13, 10);

    SnakeGridPosition head{};
    SnakeGridPosition tail{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ASSERT_TRUE(SnakeGameGetSnakeTail(&game, &tail));
    ExpectPosition(head, 15, 10);
    ExpectPosition(tail, 13, 10);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase2, StepPushesNewHeadAndRemovesOldTail)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));

    SnakeGridPosition oldHead{};
    SnakeGridPosition oldSecond{};
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 0, &oldHead));
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 1, &oldSecond));

    ASSERT_TRUE(SnakeGameStep(&game));

    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH, SnakeGameGetSnakeLength(&game));

    SnakeGridPosition segment{};
    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 0, &segment));
    ExpectPosition(segment, oldHead.x + 1, oldHead.y);

    ASSERT_TRUE(SnakeGameGetSnakeSegment(&game, 1, &segment));
    ExpectPosition(segment, oldHead.x, oldHead.y);

    ASSERT_TRUE(SnakeGameGetSnakeTail(&game, &segment));
    ExpectPosition(segment, oldSecond.x, oldSecond.y);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase2, DestroyIsSafeToCallTwice)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));

    SnakeGameDestroy(&game);
    EXPECT_EQ(0, SnakeGameGetSnakeLength(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_READY, SnakeGameGetStatus(&game));

    SnakeGameDestroy(&game);
    EXPECT_EQ(0, SnakeGameGetSnakeLength(&game));
}
