#include "gtest/gtest.h"

#include "game_core.h"

namespace
{
void ExpectPosition(const SnakeGridPosition& position, int expectedX, int expectedY)
{

    EXPECT_EQ(expectedX, position.x);
    EXPECT_EQ(expectedY, position.y);
}

void ExpectSnakeSegments(
    const SnakeGameCore* game,
    const SnakeGridPosition* expectedSegments,
    int expectedSegmentCount)
{

    ASSERT_EQ(expectedSegmentCount, SnakeGameGetSnakeLength(game));


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

    DCListDestroy(game->snakeBody);


    game->snakeBody = DCListInit();


    for (int i = 0; i < segmentCount; ++i)
    {
        DCListPushBack(game->snakeBody, segments[i]);
    }


    game->snakeLength = DCListSize(game->snakeBody);
    game->direction = direction;
    game->status = SNAKE_GAME_STATUS_RUNNING;
}


bool PositionsEqual(const SnakeGridPosition& first, const SnakeGridPosition& second)
{
    return first.x == second.x && first.y == second.y;
}


bool IsInsideCoreMap(const SnakeGridPosition& position)
{
    return position.x >= 0 &&
        position.y >= 0 &&
        position.x < SNAKE_GAME_GRID_COLUMNS &&
        position.y < SNAKE_GAME_GRID_ROWS;
}


bool IsFoodAt(const SnakeGameCore* game, const SnakeGridPosition& position)
{
    for (int i = 0; i < SnakeGameGetFoodCount(game); ++i)
    {
        SnakeGridPosition food{};
        if (SnakeGameGetFoodPosition(game, i, &food) && PositionsEqual(food, position))
        {
            return true;
        }
    }

    return false;
}


bool IsFoodOnSnake(const SnakeGameCore* game, const SnakeGridPosition& food)
{
    for (int i = 0; i < SnakeGameGetSnakeLength(game); ++i)
    {
        SnakeGridPosition segment{};
        if (SnakeGameGetSnakeSegment(game, i, &segment) && PositionsEqual(segment, food))
        {
            return true;
        }
    }

    return false;
}


void ExpectFoodsAreValid(const SnakeGameCore* game)
{
    ASSERT_GE(SnakeGameGetFoodCount(game), 0);
    ASSERT_LE(SnakeGameGetFoodCount(game), SNAKE_GAME_MAX_FOOD_COUNT);

    for (int i = 0; i < SnakeGameGetFoodCount(game); ++i)
    {
        SnakeGridPosition food{};
        ASSERT_TRUE(SnakeGameGetFoodPosition(game, i, &food));
        EXPECT_TRUE(IsInsideCoreMap(food));
        EXPECT_FALSE(IsFoodOnSnake(game, food));

        for (int j = i + 1; j < SnakeGameGetFoodCount(game); ++j)
        {
            SnakeGridPosition otherFood{};
            ASSERT_TRUE(SnakeGameGetFoodPosition(game, j, &otherFood));
            EXPECT_FALSE(PositionsEqual(food, otherFood));
        }
    }
}



bool TryArrangeSnakeForTarget(
    SnakeGameCore* game,
    const SnakeGridPosition& target,
    SnakeGridPosition* oldTail)
{
    struct DirectionVector
    {
        SnakeDirection direction;
        int dx;
        int dy;
    };

    const DirectionVector directions[] = {
        { SNAKE_DIRECTION_RIGHT, 1, 0 },
        { SNAKE_DIRECTION_LEFT, -1, 0 },
        { SNAKE_DIRECTION_DOWN, 0, 1 },
        { SNAKE_DIRECTION_UP, 0, -1 },
    };

    for (const DirectionVector& vector : directions)
    {

        const SnakeGridPosition head = { target.x - vector.dx, target.y - vector.dy };
        const SnakeGridPosition body = { head.x - vector.dx, head.y - vector.dy };
        const SnakeGridPosition tail = { body.x - vector.dx, body.y - vector.dy };

        if (!IsInsideCoreMap(head) || !IsInsideCoreMap(body) || !IsInsideCoreMap(tail))
        {
            continue;
        }


        if (IsFoodAt(game, head) || IsFoodAt(game, body) || IsFoodAt(game, tail))
        {
            continue;
        }

        const SnakeGridPosition segments[] = {
            head,
            body,
            tail,
        };
        ReplaceSnake(game, segments, 3, vector.direction);

        if (oldTail != nullptr)
        {
            *oldTail = tail;
        }

        return true;
    }

    return false;
}


bool ArrangeSnakeToEatAnyFood(
    SnakeGameCore* game,
    SnakeGridPosition* eatenFood,
    SnakeGridPosition* oldTail)
{
    for (int i = 0; i < SnakeGameGetFoodCount(game); ++i)
    {
        SnakeGridPosition food{};
        if (!SnakeGameGetFoodPosition(game, i, &food))
        {
            continue;
        }

        if (TryArrangeSnakeForTarget(game, food, oldTail))
        {
            if (eatenFood != nullptr)
            {
                *eatenFood = food;
            }

            return true;
        }
    }

    return false;
}


bool ArrangeSnakeToMoveToEmptyCell(SnakeGameCore* game, SnakeGridPosition* target)
{
    for (int y = 0; y < SNAKE_GAME_GRID_ROWS; ++y)
    {
        for (int x = 0; x < SNAKE_GAME_GRID_COLUMNS; ++x)
        {
            const SnakeGridPosition candidate = { x, y };
            if (IsFoodAt(game, candidate))
            {
                continue;
            }

            if (TryArrangeSnakeForTarget(game, candidate, nullptr))
            {
                if (target != nullptr)
                {
                    *target = candidate;
                }

                return true;
            }
        }
    }

    return false;
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
    EXPECT_EQ(SNAKE_GAME_STATUS_READY, SnakeGameGetStatus(&game));

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
    ASSERT_TRUE(SnakeGameStart(&game));

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

TEST(GameCorePhase4, RequestedDirectionsMoveHeadOneCellInThatDirection)
{
    struct DirectionCase
    {
        SnakeDirection startingDirection;
        SnakeDirection requestedDirection;
        SnakeGridPosition expectedSegments[3];
    };

    const DirectionCase cases[] = {

        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_RIGHT,
            { { 16, 10 }, { 15, 10 }, { 14, 10 } }
        },

        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_UP,
            { { 15, 9 }, { 15, 10 }, { 14, 10 } }
        },

        {
            SNAKE_DIRECTION_RIGHT,
            SNAKE_DIRECTION_DOWN,
            { { 15, 11 }, { 15, 10 }, { 14, 10 } }
        },

        {
            SNAKE_DIRECTION_UP,
            SNAKE_DIRECTION_LEFT,
            { { 14, 10 }, { 15, 10 }, { 15, 11 } }
        },
    };

    for (const DirectionCase& testCase : cases)
    {
        SnakeGameCore game{};

        ASSERT_TRUE(SnakeGameInit(&game));
        ASSERT_TRUE(SnakeGameStart(&game));

        if (testCase.requestedDirection == SNAKE_DIRECTION_LEFT)
        {

            const SnakeGridPosition verticalSnake[] = {
                { 15, 10 },
                { 15, 11 },
                { 15, 12 },
            };
            ReplaceSnake(&game, verticalSnake, 3, testCase.startingDirection);
        }
        else
        {

            game.direction = testCase.startingDirection;
        }


        ASSERT_TRUE(SnakeGameRequestDirection(&game, testCase.requestedDirection));
        ASSERT_TRUE(SnakeGameStep(&game));


        ExpectSnakeSegments(&game, testCase.expectedSegments, SNAKE_GAME_INITIAL_LENGTH);

        SnakeGameDestroy(&game);
    }
}

TEST(GameCorePhase4, DirectionRequestCoversAllCurrentAndRequestedDirectionCombinations)
{
    struct DirectionRequestCase
    {

        SnakeDirection startingDirection;


        SnakeDirection requestedDirection;



        SnakeDirection expectedDirection;
    };

    const DirectionRequestCase cases[] = {

        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT },
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_UP, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT },


        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT },
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_RIGHT },


        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_RIGHT },
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT },


        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_UP, SNAKE_DIRECTION_UP },
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_RIGHT, SNAKE_DIRECTION_LEFT },
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_DOWN, SNAKE_DIRECTION_DOWN },
        { SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT, SNAKE_DIRECTION_LEFT },
    };

    for (const DirectionRequestCase& testCase : cases)
    {
        SnakeGameCore game{};
        ASSERT_TRUE(SnakeGameInit(&game));


        game.direction = testCase.startingDirection;


        ASSERT_TRUE(SnakeGameRequestDirection(&game, testCase.requestedDirection));


        EXPECT_EQ(testCase.expectedDirection, SnakeGameGetDirection(&game));


        SnakeGameDestroy(&game);
    }
}
TEST(GameCorePhase4, DirectReverseDirectionRequestIsIgnored)
{
    struct ReverseCase
    {

        SnakeDirection startingDirection;


        SnakeDirection requestedDirection;



        SnakeGridPosition initialSegments[3];



        SnakeGridPosition expectedSegmentsAfterStep[3];
    };

    const ReverseCase cases[] = {

        {
            SNAKE_DIRECTION_UP,
            SNAKE_DIRECTION_DOWN,
            { { 15, 10 }, { 15, 11 }, { 15, 12 } },
            { { 15, 9 }, { 15, 10 }, { 15, 11 } }
        },

        {
            SNAKE_DIRECTION_DOWN,
            SNAKE_DIRECTION_UP,
            { { 15, 10 }, { 15, 9 }, { 15, 8 } },
            { { 15, 11 }, { 15, 10 }, { 15, 9 } }
        },

        {
            SNAKE_DIRECTION_LEFT,
            SNAKE_DIRECTION_RIGHT,
            { { 15, 10 }, { 16, 10 }, { 17, 10 } },
            { { 14, 10 }, { 15, 10 }, { 16, 10 } }
        },

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


        ReplaceSnake(&game, testCase.initialSegments, SNAKE_GAME_INITIAL_LENGTH, testCase.startingDirection);


        ASSERT_TRUE(SnakeGameRequestDirection(&game, testCase.requestedDirection));


        EXPECT_EQ(testCase.startingDirection, SnakeGameGetDirection(&game));


        ASSERT_TRUE(SnakeGameStep(&game));


        ExpectSnakeSegments(&game, testCase.expectedSegmentsAfterStep, SNAKE_GAME_INITIAL_LENGTH);

        SnakeGameDestroy(&game);
    }
}

TEST(GameCorePhase4, NonReverseTurnRequestsChangeDirection)
{
    SnakeGameCore game{};

    ASSERT_TRUE(SnakeGameInit(&game));


    ASSERT_TRUE(SnakeGameRequestDirection(&game, SNAKE_DIRECTION_UP));
    EXPECT_EQ(SNAKE_DIRECTION_UP, SnakeGameGetDirection(&game));


    ASSERT_TRUE(SnakeGameRequestDirection(&game, SNAKE_DIRECTION_RIGHT));
    EXPECT_EQ(SNAKE_DIRECTION_RIGHT, SnakeGameGetDirection(&game));


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

        { { { 0, 5 }, { 1, 5 }, { 2, 5 } }, SNAKE_DIRECTION_LEFT },

        { { { 29, 5 }, { 28, 5 }, { 27, 5 } }, SNAKE_DIRECTION_RIGHT },

        { { { 5, 0 }, { 5, 1 }, { 5, 2 } }, SNAKE_DIRECTION_UP },

        { { { 5, 19 }, { 5, 18 }, { 5, 17 } }, SNAKE_DIRECTION_DOWN },
    };

    for (const WallCase& testCase : cases)
    {
        SnakeGameCore game{};
        ASSERT_TRUE(SnakeGameInit(&game));

        ReplaceSnake(&game, testCase.segments, 3, testCase.direction);


        ASSERT_TRUE(SnakeGameStep(&game));
        EXPECT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

        SnakeGameDestroy(&game);
    }
}

TEST(GameCorePhase4, DeadSnakeDoesNotMoveOnLaterSteps)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));


    const SnakeGridPosition segments[] = {
        { 29, 5 },
        { 28, 5 },
        { 27, 5 },
    };
    ReplaceSnake(&game, segments, 3, SNAKE_DIRECTION_RIGHT);


    ASSERT_TRUE(SnakeGameStep(&game));
    ASSERT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

    SnakeGridPosition headAfterDeath{};

    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfterDeath));
    const int lengthAfterDeath = SnakeGameGetSnakeLength(&game);


    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition headAfterLaterStep{};

    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfterLaterStep));
    ExpectPosition(headAfterLaterStep, headAfterDeath.x, headAfterDeath.y);
    EXPECT_EQ(lengthAfterDeath, SnakeGameGetSnakeLength(&game));

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase4, HittingNonTailBodySegmentKillsSnake)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));


    const SnakeGridPosition segments[] = {
        { 5, 5 },
        { 5, 4 },
        { 4, 4 },
        { 4, 5 },
    };
    ReplaceSnake(&game, segments, 4, SNAKE_DIRECTION_UP);


    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase4, MovingIntoOldTailCellIsAllowedBecauseTailMovesAway)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInit(&game));


    const SnakeGridPosition segments[] = {
        { 5, 5 },
        { 5, 4 },
        { 4, 4 },
        { 4, 5 },
    };
    ReplaceSnake(&game, segments, 4, SNAKE_DIRECTION_LEFT);


    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));
    EXPECT_EQ(4, SnakeGameGetSnakeLength(&game));

    SnakeGridPosition head{};

    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ExpectPosition(head, 4, 5);

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, SeededInitializationCreatesFiveValidFoods)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 12345u));

    EXPECT_EQ(0, SnakeGameGetScore(&game));
    EXPECT_EQ(0, SnakeGameGetFoodsEaten(&game));
    EXPECT_EQ(1, SnakeGameGetLevel(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_MOVE_INTERVAL_MS, SnakeGameGetMoveIntervalMs(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_FOOD_COUNT, SnakeGameGetFoodCount(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_FOOD_COUNT, SnakeGameGetTargetFoodCount(&game));
    ExpectFoodsAreValid(&game);

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, EatingFoodIncreasesScoreAndKeepsOldTail)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 24680u));

    SnakeGridPosition eatenFood{};
    SnakeGridPosition oldTail{};
    ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, &eatenFood, &oldTail));

    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));
    EXPECT_EQ(1, SnakeGameGetScore(&game));
    EXPECT_EQ(1, SnakeGameGetFoodsEaten(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH + 1, SnakeGameGetSnakeLength(&game));

    SnakeGridPosition head{};
    SnakeGridPosition tail{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ASSERT_TRUE(SnakeGameGetSnakeTail(&game, &tail));
    ExpectPosition(head, eatenFood.x, eatenFood.y);
    ExpectPosition(tail, oldTail.x, oldTail.y);

    EXPECT_EQ(SNAKE_GAME_INITIAL_FOOD_COUNT, SnakeGameGetFoodCount(&game));
    ExpectFoodsAreValid(&game);

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, NormalMoveWithoutFoodKeepsLengthAndScore)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 13579u));

    SnakeGridPosition target{};
    ASSERT_TRUE(ArrangeSnakeToMoveToEmptyCell(&game, &target));

    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH, SnakeGameGetSnakeLength(&game));
    EXPECT_EQ(0, SnakeGameGetScore(&game));
    EXPECT_EQ(0, SnakeGameGetFoodsEaten(&game));

    SnakeGridPosition head{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ExpectPosition(head, target.x, target.y);
    ExpectFoodsAreValid(&game);

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, EatingFiveFoodsRaisesLevelAndUpdatesSpeedAndFoodTarget)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 777u));

    for (int i = 0; i < SNAKE_GAME_FOODS_PER_LEVEL; ++i)
    {
        ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, nullptr, nullptr));
        ASSERT_TRUE(SnakeGameStep(&game));
        ExpectFoodsAreValid(&game);
    }

    EXPECT_EQ(5, SnakeGameGetScore(&game));
    EXPECT_EQ(5, SnakeGameGetFoodsEaten(&game));
    EXPECT_EQ(2, SnakeGameGetLevel(&game));
    EXPECT_EQ(170, SnakeGameGetMoveIntervalMs(&game));
    EXPECT_EQ(6, SnakeGameGetTargetFoodCount(&game));
    EXPECT_EQ(6, SnakeGameGetFoodCount(&game));

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, FoodTargetCapsAtTwelveAndSpeedNeverDropsBelowFloor)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 98765u));

    for (int i = 0; i < 70; ++i)
    {
        ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, nullptr, nullptr));
        ASSERT_TRUE(SnakeGameStep(&game));

        const int expectedLevel = SnakeGameGetFoodsEaten(&game) / SNAKE_GAME_FOODS_PER_LEVEL + 1;
        int expectedTargetFoodCount = SNAKE_GAME_INITIAL_FOOD_COUNT + expectedLevel / SNAKE_GAME_FOOD_GROWTH_LEVEL_INTERVAL;
        if (expectedTargetFoodCount > SNAKE_GAME_MAX_FOOD_COUNT)
        {
            expectedTargetFoodCount = SNAKE_GAME_MAX_FOOD_COUNT;
        }

        EXPECT_EQ(expectedLevel, SnakeGameGetLevel(&game));
        EXPECT_EQ(expectedTargetFoodCount, SnakeGameGetTargetFoodCount(&game));
        EXPECT_EQ(expectedTargetFoodCount, SnakeGameGetFoodCount(&game));
        EXPECT_GE(SnakeGameGetMoveIntervalMs(&game), SNAKE_GAME_MIN_MOVE_INTERVAL_MS);
        ExpectFoodsAreValid(&game);
    }

    EXPECT_EQ(15, SnakeGameGetLevel(&game));
    EXPECT_EQ(SNAKE_GAME_MAX_FOOD_COUNT, SnakeGameGetTargetFoodCount(&game));
    EXPECT_EQ(SNAKE_GAME_MAX_FOOD_COUNT, SnakeGameGetFoodCount(&game));
    EXPECT_EQ(SNAKE_GAME_MIN_MOVE_INTERVAL_MS, SnakeGameGetMoveIntervalMs(&game));

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, SnakeColorStartsGreen)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 1122u));

    EXPECT_EQ(SNAKE_COLOR_MODE_GREEN, SnakeGameGetSnakeColorMode(&game));

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, NormalMoveKeepsSnakeGreen)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 2233u));

    ASSERT_TRUE(ArrangeSnakeToMoveToEmptyCell(&game, nullptr));
    ASSERT_TRUE(SnakeGameStep(&game));

    EXPECT_EQ(SNAKE_COLOR_MODE_GREEN, SnakeGameGetSnakeColorMode(&game));

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, SnakeStartsGreenAndTurnsRedAfterEatingFood)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 3344u));
    ASSERT_EQ(SNAKE_COLOR_MODE_GREEN, SnakeGameGetSnakeColorMode(&game));

    ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, nullptr, nullptr));
    ASSERT_TRUE(SnakeGameStep(&game));

    EXPECT_EQ(SNAKE_COLOR_MODE_RED, SnakeGameGetSnakeColorMode(&game));

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, RedSnakeStaysRedAfterLaterNormalMove)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 4455u));

    ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, nullptr, nullptr));
    ASSERT_TRUE(SnakeGameStep(&game));
    ASSERT_EQ(SNAKE_COLOR_MODE_RED, SnakeGameGetSnakeColorMode(&game));

    ASSERT_TRUE(ArrangeSnakeToMoveToEmptyCell(&game, nullptr));
    ASSERT_TRUE(SnakeGameStep(&game));

    EXPECT_EQ(SNAKE_COLOR_MODE_RED, SnakeGameGetSnakeColorMode(&game));

    SnakeGameDestroy(&game);
}


TEST(GameCorePhase5, RedSnakeStaysRedAfterDeath)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 5566u));

    ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, nullptr, nullptr));
    ASSERT_TRUE(SnakeGameStep(&game));
    ASSERT_EQ(SNAKE_COLOR_MODE_RED, SnakeGameGetSnakeColorMode(&game));

    const SnakeGridPosition wallCrashSnake[] = {
        { 29, 5 },
        { 28, 5 },
        { 27, 5 },
    };
    ReplaceSnake(&game, wallCrashSnake, 3, SNAKE_DIRECTION_RIGHT);

    ASSERT_TRUE(SnakeGameStep(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));
    EXPECT_EQ(SNAKE_COLOR_MODE_RED, SnakeGameGetSnakeColorMode(&game));

    SnakeGameDestroy(&game);
}
TEST(GameCorePhase6, ReadyGameDoesNotMoveUntilStarted)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 6060u));
    ASSERT_EQ(SNAKE_GAME_STATUS_READY, SnakeGameGetStatus(&game));

    SnakeGridPosition headBefore{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headBefore));

    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition headAfter{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfter));
    ExpectPosition(headAfter, headBefore.x, headBefore.y);
    EXPECT_EQ(SNAKE_GAME_STATUS_READY, SnakeGameGetStatus(&game));

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase6, StartMovesReadyGameIntoRunningAndAllowsMovement)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 6161u));

    SnakeGridPosition oldHead{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &oldHead));

    ASSERT_TRUE(SnakeGameStart(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));

    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition newHead{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &newHead));
    ExpectPosition(newHead, oldHead.x + 1, oldHead.y);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase6, PauseFreezesSnakeAndResumeAllowsMovement)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 6262u));
    ASSERT_TRUE(SnakeGameStart(&game));

    ASSERT_TRUE(SnakeGameTogglePause(&game));
    ASSERT_EQ(SNAKE_GAME_STATUS_PAUSED, SnakeGameGetStatus(&game));

    SnakeGridPosition pausedHead{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &pausedHead));
    const int pausedLength = SnakeGameGetSnakeLength(&game);

    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition afterPausedStep{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &afterPausedStep));
    ExpectPosition(afterPausedStep, pausedHead.x, pausedHead.y);
    EXPECT_EQ(pausedLength, SnakeGameGetSnakeLength(&game));

    ASSERT_TRUE(SnakeGameTogglePause(&game));
    ASSERT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));
    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition afterResumeStep{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &afterResumeStep));
    ExpectPosition(afterResumeStep, pausedHead.x + 1, pausedHead.y);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase6, PauseCommandDoesNotReviveDeadSnake)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 6363u));

    const SnakeGridPosition wallCrashSnake[] = {
        { 29, 5 },
        { 28, 5 },
        { 27, 5 },
    };
    ReplaceSnake(&game, wallCrashSnake, 3, SNAKE_DIRECTION_RIGHT);

    ASSERT_TRUE(SnakeGameStep(&game));
    ASSERT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

    ASSERT_TRUE(SnakeGameTogglePause(&game));
    EXPECT_EQ(SNAKE_GAME_STATUS_DEAD, SnakeGameGetStatus(&game));

    SnakeGridPosition headAfterDeath{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfterDeath));

    ASSERT_TRUE(SnakeGameStep(&game));

    SnakeGridPosition headAfterLaterStep{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &headAfterLaterStep));
    ExpectPosition(headAfterLaterStep, headAfterDeath.x, headAfterDeath.y);

    SnakeGameDestroy(&game);
}

TEST(GameCorePhase6, RestartResetsRoundAndEntersRunning)
{
    SnakeGameCore game{};
    ASSERT_TRUE(SnakeGameInitWithSeed(&game, 6464u));
    ASSERT_TRUE(SnakeGameStart(&game));

    ASSERT_TRUE(ArrangeSnakeToEatAnyFood(&game, nullptr, nullptr));
    ASSERT_TRUE(SnakeGameStep(&game));
    ASSERT_GT(SnakeGameGetScore(&game), 0);
    ASSERT_GT(SnakeGameGetSnakeLength(&game), SNAKE_GAME_INITIAL_LENGTH);

    ASSERT_TRUE(SnakeGameRestart(&game));

    EXPECT_EQ(SNAKE_GAME_STATUS_RUNNING, SnakeGameGetStatus(&game));
    EXPECT_EQ(0, SnakeGameGetScore(&game));
    EXPECT_EQ(0, SnakeGameGetFoodsEaten(&game));
    EXPECT_EQ(1, SnakeGameGetLevel(&game));
    EXPECT_EQ(SNAKE_DIRECTION_RIGHT, SnakeGameGetDirection(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_LENGTH, SnakeGameGetSnakeLength(&game));
    EXPECT_EQ(SNAKE_GAME_INITIAL_FOOD_COUNT, SnakeGameGetFoodCount(&game));
    ExpectFoodsAreValid(&game);

    SnakeGridPosition head{};
    ASSERT_TRUE(SnakeGameGetSnakeHead(&game, &head));
    ExpectPosition(head, 15, 10);

    SnakeGameDestroy(&game);
}