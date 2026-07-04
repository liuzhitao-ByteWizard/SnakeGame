#define _CRT_SECURE_NO_WARNINGS

#include "DCList.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static DCListNode* BuyDCListNode(DCLDataType data)
{
    /*
        链表节点必须从堆上申请内存，因为蛇身节点要在本函数返回后继续存在。
        如果这里使用局部变量，函数结束后局部变量会失效，链表中保存的指针
        就会变成悬空指针。
    */
    DCListNode* newNode = (DCListNode*)malloc(sizeof(DCListNode));
    if (newNode == NULL)
    {
        printf("BuyDCListNode failed.\n");
        exit(EXIT_FAILURE);
    }

    newNode->data = data;
    newNode->prev = NULL;
    newNode->next = NULL;
    return newNode;
}

DCListNode* DCListInit(void)
{
    /*
        本链表使用“哨兵头节点”。哨兵节点不表示蛇身的一节，它只是一个
        固定锚点，让空链表、头插、尾删都能使用统一的指针规则。

        在空的循环链表中，哨兵节点的前后指针都指向自己：

            L->next == L
            L->prev == L

        插入真实蛇身节点后，L->next 就是蛇头，L->prev 就是蛇尾。
    */
    const SnakeGridPosition sentinelValue = { -1, -1 };
    DCListNode* L = BuyDCListNode(sentinelValue);
    L->next = L;
    L->prev = L;
    return L;
}

void DCListDestroy(DCListNode* L)
{
    if (L == NULL)
    {
        return;
    }

    DCListNode* cur = L->next;
    while (cur != L)
    {
        DCListNode* next = cur->next;
        free(cur);
        cur = next;
    }

    free(L);
}

DCListNode* DCListGetElem(DCListNode* L, int i)
{
    assert(L);
    assert(i >= 0);

    DCListNode* cur = L->next;
    int j = 0;
    while (cur != L && j < i)
    {
        cur = cur->next;
        ++j;
    }

    assert(cur != L);
    assert(j == i);
    return cur;
}

const DCListNode* DCListGetConstElem(const DCListNode* L, int i)
{
    assert(L);
    assert(i >= 0);

    const DCListNode* cur = L->next;
    int j = 0;
    while (cur != L && j < i)
    {
        cur = cur->next;
        ++j;
    }

    assert(cur != L);
    assert(j == i);
    return cur;
}

void DCListInsert(DCListNode* pos, DCLDataType x)
{
    assert(pos);

    /*
        在 pos 节点后插入新节点。

        对蛇的移动来说，在哨兵节点 L 后插入就是“头插”：新节点会成为
        L->next，也就是游戏核心读取到的蛇头。
    */
    DCListNode* newNode = BuyDCListNode(x);

    newNode->next = pos->next;
    pos->next->prev = newNode;

    pos->next = newNode;
    newNode->prev = pos;
}

void DCListDelete(DCListNode* pos)
{
    assert(pos);
    assert(pos->next);
    assert(pos->prev);

    pos->prev->next = pos->next;
    pos->next->prev = pos->prev;

    free(pos);
}

void DCListPushFront(DCListNode* L, DCLDataType x)
{
    assert(L);
    DCListInsert(L, x);
}

void DCListPushBack(DCListNode* L, DCLDataType x)
{
    assert(L);
    DCListInsert(L->prev, x);
}

void DCListPopFront(DCListNode* L)
{
    assert(L);
    assert(L->next != L);
    DCListDelete(L->next);
}

void DCListPopBack(DCListNode* L)
{
    assert(L);
    assert(L->prev != L);
    DCListDelete(L->prev);
}

int DCListSize(const DCListNode* L)
{
    if (L == NULL)
    {
        return 0;
    }

    int count = 0;
    const DCListNode* cur = L->next;
    while (cur != L)
    {
        ++count;
        cur = cur->next;
    }

    return count;
}

void DCListPrint(const DCListNode* L)
{
    if (L == NULL)
    {
        printf("(null)\n");
        return;
    }

    const DCListNode* cur = L->next;
    while (cur != L)
    {
        printf("(%d,%d)->", cur->data.x, cur->data.y);
        cur = cur->next;
    }

    printf("\n");
}
