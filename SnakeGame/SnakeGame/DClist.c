#define _CRT_SECURE_NO_WARNINGS

#include "DCList.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static DCListNode* BuyDCListNode(DCLDataType data)
{
    /*
        链表节点必须从堆上申请。

        蛇身节点会在函数返回后继续留在链表里，如果使用局部变量，
        函数结束后节点地址就失效了，蛇身链表会变成悬空指针。
        所以每一节蛇身都通过 malloc 创建，并在删除节点或销毁链表时释放。
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
        本链表使用哨兵节点。哨兵节点不代表蛇身的一节，
        它只是一个固定锚点，让空链表、头插、尾删都能走统一逻辑。

        空链表时：L->next == L，并且 L->prev == L。
        插入蛇身后：L->next 是蛇头，L->prev 是蛇尾。
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

        对贪吃蛇来说，如果 pos 是哨兵节点 L，那么这里就是头插：
        新节点会成为 L->next，也就是游戏核心读取到的新蛇头。
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
