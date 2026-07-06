#pragma once
#ifndef SNAKE_GAME_HIGH_SCORE_INCLUDED
#define SNAKE_GAME_HIGH_SCORE_INCLUDED

/* 引入 bool 类型，让保存函数可以明确告诉调用者“写入是否成功”。 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    最高分存档模块故意和 game_core 分开。

    game_core 只负责贪吃蛇的核心规则：移动、碰撞、食物、分数和等级。
    文本文件读写属于“把规则结果保存到本地”的外围功能，不应该塞进核心规则层。
    这样做的好处是：核心层仍然可以不依赖文件系统、不依赖 raylib，后续测试也更干净。
*/
int SnakeHighScoreLoad(const char* filePath);

/*
    保存失败时返回 false，但调用者不应该因此中断游戏。

    玩家本局已经获得了分数，磁盘写入失败只代表“下次启动可能记不住这个最高分”。
    当前界面仍然可以继续显示内存里的最高分，保证游戏体验不被存档问题打断。
*/
bool SnakeHighScoreSave(const char* filePath, int highScore);

#ifdef __cplusplus
}
#endif

#endif
