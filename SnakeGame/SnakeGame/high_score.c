#include "high_score.h"

/*
    MSVC 会对标准 C 的 fopen 给出安全警告，并建议使用 fopen_s。

    本项目规格要求使用简单文本文件做本地存档，且项目按 C11 编写。
    因此这里保留标准 C 文件接口，只在本文件局部关闭这个 MSVC 专属提示。
*/
#define _CRT_SECURE_NO_WARNINGS

/* 引入字符分类函数，用来判断空白字符和数字字符。 */
#include <ctype.h>

/* 引入 INT_MAX，用来防止文本数字超过 int 能表示的最大值。 */
#include <limits.h>

/* 引入标准文件读写接口，用来读取和写入 highscore.txt。 */
#include <stdio.h>

static bool IsWhitespace(char character)
{
    /* 把 char 转成 unsigned char 后再交给 isspace，避免负 char 值触发未定义行为。 */
    return isspace((unsigned char)character) != 0;
}

static bool IsDigit(char character)
{
    /* 把 char 转成 unsigned char 后再交给 isdigit，保证字符分类调用是安全的。 */
    return isdigit((unsigned char)character) != 0;
}

static bool TryParseNonNegativeInt(const char* text, int* outValue)
{
    /* cursor 指向当前正在检查的字符，后续会从左到右扫描整个文本。 */
    const char* cursor = text;

    /* value 先用更大的 long long 保存，方便在转成 int 前检查是否溢出。 */
    long long value = 0;

    /* sawDigit 用来区分“真的读到了数字”和“文件只有空白内容”。 */
    bool sawDigit = false;

    /* 先检查入参是否有效，避免后面解引用空指针。 */
    if (text == NULL || outValue == NULL)
    {
        /* 入参无效时解析失败，调用者会按 0 分处理。 */
        return false;
    }

    /*
        存档文件允许玩家手动打开修改，所以解析必须比 atoi 更严格。

        atoi("12abc") 会悄悄返回 12，但这类内容其实已经不是合法的“只保存一个整数”。
        因此这里只接受：开头空白、连续数字、结尾空白。
        只要中间混入其他字符，就认为文件内容非法并回退到 0。
    */

    /* 跳过数字前面的空白，让文件里写成“  7”也能被识别为合法最高分。 */
    while (*cursor != '\0' && IsWhitespace(*cursor))
    {
        /* 每跳过一个空白字符，就把 cursor 移到下一个字符。 */
        ++cursor;
    }

    /* 连续读取数字字符，直到遇到非数字或字符串结尾。 */
    while (*cursor != '\0' && IsDigit(*cursor))
    {
        /* 记录至少读到过一个数字，否则空文件或纯空白文件不能算合法整数。 */
        sawDigit = true;

        /* 把当前数字追加到 value 末尾，例如读到 '5' 时就等价于 value = value * 10 + 5。 */
        value = value * 10 + (*cursor - '0');

        /* 如果数值超过 int 上限，就认为文件非法，避免后续强制转换出错。 */
        if (value > INT_MAX)
        {
            /* 溢出的数字不能作为最高分使用，交给调用者按 0 处理。 */
            return false;
        }

        /* 当前数字处理完毕，继续检查下一个字符。 */
        ++cursor;
    }

    /* 数字后面允许有换行或空格，这样 fprintf 写出的“7\n”能正常读取。 */
    while (*cursor != '\0' && IsWhitespace(*cursor))
    {
        /* 每跳过一个结尾空白字符，就把 cursor 移到下一个字符。 */
        ++cursor;
    }

    /* 如果没有读到数字，或者跳过结尾空白后还有其他字符，就说明内容非法。 */
    if (!sawDigit || *cursor != '\0')
    {
        /* 非法内容不能被当成最高分，调用者会回退到 0。 */
        return false;
    }

    /* 通过所有检查后，才把解析结果写给调用者。 */
    *outValue = (int)value;

    /* 返回 true 表示文本确实是一个合法的非负整数。 */
    return true;
}

int SnakeHighScoreLoad(const char* filePath)
{
    /* buffer 保存从文本文件里读出的原始字符，64 字节对一个整数存档已经足够。 */
    char buffer[64];

    /* bytesRead 记录实际读到了多少字节，后面要用它手动补字符串结束符。 */
    size_t bytesRead = 0;

    /* highScore 保存解析后的最高分，默认值为 0。 */
    int highScore = 0;

    /* file 保存打开后的文件句柄，打开失败时保持 NULL。 */
    FILE* file = NULL;

    /* 先检查文件路径是否有效，避免 fopen 收到空指针。 */
    if (filePath == NULL)
    {
        /* 没有路径就等价于没有可用存档，按 0 分处理。 */
        return 0;
    }

    /*
        文件不存在、文件被占用、内容损坏，对玩家来说都表示“没有可信的历史最高分”。

        因此读取失败不弹错误、不阻止启动，而是统一返回 0。
        这样游戏可以始终顺利打开，存档只是锦上添花。
    */

    /* 以只读文本模式打开最高分文件。 */
    file = fopen(filePath, "r");

    /* 如果文件打不开，说明当前没有可用存档。 */
    if (file == NULL)
    {
        /* 打不开文件时按 0 分处理。 */
        return 0;
    }

    /* 最多读取 buffer 容量减 1 的字节，预留一个位置给字符串结束符。 */
    bytesRead = fread(buffer, sizeof(char), sizeof(buffer) - 1, file);

    /* 检查读取过程中是否发生错误。 */
    if (ferror(file) != 0)
    {
        /* 发生读取错误时先关闭文件，避免文件句柄泄漏。 */
        fclose(file);

        /* 读取失败时按 0 分处理。 */
        return 0;
    }

    /* 如果还没到文件结尾，说明文件太长，不符合“只保存一个整数”的简单格式。 */
    if (!feof(file))
    {
        /* 文件太长也要先关闭文件句柄。 */
        fclose(file);

        /* 超长内容按非法存档处理，返回 0。 */
        return 0;
    }

    /* 文件内容已经读完，可以关闭文件句柄。 */
    fclose(file);

    /* 给 fread 读出的字符补上字符串结束符，后面的解析函数才能安全扫描。 */
    buffer[bytesRead] = '\0';

    /* 严格解析文件内容，确认它确实只有一个非负整数。 */
    if (!TryParseNonNegativeInt(buffer, &highScore))
    {
        /* 解析失败时按规格回退到 0。 */
        return 0;
    }

    /* 解析成功时返回文件中保存的最高分。 */
    return highScore;
}

bool SnakeHighScoreSave(const char* filePath, int highScore)
{
    /* file 保存打开后的文件句柄，写入失败时保持 NULL。 */
    FILE* file = NULL;

    /* 写入前先检查路径和分数，保证不会保存负数这种非法最高分。 */
    if (filePath == NULL || highScore < 0)
    {
        /* 参数非法时返回 false，让调用者知道这次保存没有成功。 */
        return false;
    }

    /*
        存档文件只写一个普通整数和一个换行。

        这种格式便于人工查看、人工修复，也完全满足 Phase 7 的需求。
        如果以后要保存更多内容，再扩展成结构化格式也不迟。
    */

    /* 以写入文本模式打开文件；如果文件已存在，会用新的最高分覆盖旧内容。 */
    file = fopen(filePath, "w");

    /* 如果文件打不开，说明当前目录不可写或文件被占用。 */
    if (file == NULL)
    {
        /* 写入失败时返回 false，但游戏流程不需要因此中断。 */
        return false;
    }

    /* 把最高分写成一个整数加换行，保持文本文件简单清晰。 */
    if (fprintf(file, "%d\n", highScore) < 0)
    {
        /* 写入失败时先关闭文件，避免文件句柄泄漏。 */
        fclose(file);

        /* 返回 false 告诉调用者本次保存失败。 */
        return false;
    }

    /* fclose 也可能失败，比如系统还没能把缓冲区内容真正写到磁盘。 */
    if (fclose(file) != 0)
    {
        /* 关闭失败时同样认为本次保存不可靠。 */
        return false;
    }

    /* 所有写入步骤都成功，返回 true。 */
    return true;
}
