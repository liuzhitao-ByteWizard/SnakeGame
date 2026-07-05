#ifndef SNAKE_DCLIST_H
#define SNAKE_DCLIST_H

#include "grid_position.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    双向循环链表是蛇身的实际存储结构。

    每个链表节点保存一个网格坐标：
      - L->next 是第一个真实节点，游戏把它当作蛇头。
      - L->prev 是最后一个真实节点，游戏把它当作蛇尾。

    这和经典贪吃蛇的移动方式非常贴合。普通移动时，游戏只需要在头部
    插入一个新蛇头，再删除尾部的一节。链表可以通过局部指针修改完成
    这两个操作，不需要像数组那样整体搬移蛇身数据。
*/
typedef SnakeGridPosition DCLDataType;

typedef struct DCListNode
{
    DCLDataType data;
    struct DCListNode* prev;
    struct DCListNode* next;
} DCListNode;

DCListNode* DCListInit(void);
void DCListDestroy(DCListNode* L);

DCListNode* DCListGetElem(DCListNode* L, int i);
const DCListNode* DCListGetConstElem(const DCListNode* L, int i);

void DCListInsert(DCListNode* pos, DCLDataType x);
void DCListDelete(DCListNode* pos);

void DCListPushFront(DCListNode* L, DCLDataType x);
void DCListPushBack(DCListNode* L, DCLDataType x);
void DCListPopFront(DCListNode* L);
void DCListPopBack(DCListNode* L);

int DCListSize(const DCListNode* L);
void DCListPrint(const DCListNode* L);

#ifdef __cplusplus
}
#endif

#endif
