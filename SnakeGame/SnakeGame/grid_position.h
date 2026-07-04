#ifndef SNAKE_GRID_POSITION_H
#define SNAKE_GRID_POSITION_H

/*
    蛇身的一节只关心它位于游戏网格中的哪一格，而不关心屏幕像素位置。

    界面层以后会把这个网格坐标转换成像素坐标；游戏核心层只需要知道
    “第几列”和“第几行”。这个类型不包含 raylib 内容，所以 Google Test
    项目可以单独编译核心规则，不需要链接窗口和绘图库。
*/
typedef struct SnakeGridPosition
{
    int x;
    int y;
} SnakeGridPosition;

#endif
