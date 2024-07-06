#include "zip.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>

#define NUM 256//字符种类（该阈值应不小于文档原有字符种类）
#define MAXSIZE 1000000 //文件字符量(该阈值不大于文档原有字符量)

typedef struct
{
    char ch;
    union
    {
        int count;
        char* HC;
    };
} kind;

typedef struct
{
    kind weight;
    int parent, lchild, rchild;
} HTNode, * HuffmanTree;

int kinds(char* str, kind* ch)
{
    int i, j, k = 0, flag, len = strlen(str);

    for (i = 0; i < len; i++)
    {
        flag = 0;
        for (j = 0; j < k; j++)
        {
            if (str[i] == ch[j].ch)
            {
                flag = 1;
                ch[j].count++;
                break;
            }
        }
        if (flag == 0)
        {
            ch[k].ch = str[i];
            ch[k].count = 1;
            k++;
        }
    }
    for (i = 0; i < k; i++)
    {
        if (ch[i].ch == '1')
        {
            kind temp = ch[i];
            ch[i] = ch[0];
            ch[0] = temp;
        }
        if (ch[i].ch == '0')
        {
            ch[i].ch = 'O';
            kind temp = ch[i];
            ch[i] = ch[k - 1];
            ch[k - 1] = temp;
        }
    }
    return k;//返回字符种类数
}

void Select(HuffmanTree HT, int num, int& s1, int& s2)
{
    int i;
    int min1, min2;
    min1 = min2 = 32767;
    for (i = 1; i <= num; i++)
    {
        if (HT[i].parent == 0 && HT[i].weight.count < min1)//    count  |  min1   min2
        {
            min2 = min1;
            s2 = s1;
            min1 = HT[i].weight.count;
            s1 = i;
        }
        else if (HT[i].parent == 0 && HT[i].weight.count < min2)//   min1  |  count  |   min2
        {
            min2 = HT[i].weight.count;
            s2 = i;
        }
    }
}

void CreatHuffmanTree(HuffmanTree& HT, int num, kind* ch)
{
    if (num <= 1)
    {
        printf("Error\n");
        return;
    }
    int m, i, s1, s2;
    m = 2 * num - 1;
    HT = new HTNode[m + 1];
    for (i = 1; i <= m; i++)
    {
        HT[i].weight = ch[i - 1];
        HT[i].parent = 0;
        HT[i].lchild = 0;
        HT[i].rchild = 0;
    }
    //-----------初始化-------------

    //-----------构造哈夫曼树---------
    for (i = num + 1; i <= m; i++)
    {
        Select(HT, i - 1, s1, s2);
        HT[s1].parent = i;
        HT[s2].parent = i;
        HT[i].lchild = s1;
        HT[i].rchild = s2;
        HT[i].weight.ch = '*';
        HT[i].weight.count = HT[s1].weight.count + HT[s2].weight.count;
    }
}

void CreatHuffmanCode(HuffmanTree HT, kind* ch, int num)
{
    char* cd;
    cd = new char[num];
    cd[num - 1] = '\0';
    int start, c, p, i;
    for (i = 1; i <= num; i++)
    {
        start = num - 1;//从后往前
        for (c = i, p = HT[i].parent; p != 0; c = p, p = HT[p].parent)//c当前child节点，p父parent节点
        {
            if (HT[p].lchild == c)//左0右1
            {
                cd[--start] = '0';
            }
            else
            {
                cd[--start] = '1';
            }
        }
        ch[i - 1].HC = new char[num - start];
        strcpy(ch[i - 1].HC, &cd[start]);
    }
    delete cd;
}

void encode(char* str, kind* ch, char* encode_str, int num)
{
    int len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        if (str[i] == '0')
        {
            str[i] = 'O';
        }
        for (int j = 0; j < num; j++)
        {
            if (str[i] == ch[j].ch)
            {
                encode_str = strcat(encode_str, ch[j].HC);
            }
        }
    }
}

void write_huffman_code(FILE* file, kind ch)
{
    fprintf(file, "%c%s", ch.ch, ch.HC);  // 写入字符    
}

void writeBitsToFile(char* bits, FILE* file) // 将二进制字符串按位写入文件
{
    unsigned char byte = 0;
    int Count = 0;
    int len = strlen(bits);
    for (int i = 0; i < len; i++)
    {
        byte = (byte << 1) | (bits[i] - '0');
        if (++Count == 8)
        {
            fwrite(&byte, sizeof(byte), 1, file);
            byte = 0;
            Count = 0;
        }
    }
    // 如果还有剩余的Count位没有写入，将它们写入文件
    if (Count > 0)
    {
        byte = (byte << 1) | 1;
        byte <<= (7 - Count);
        fwrite(&byte, sizeof(byte), 1, file);
    }
}

void read_huffman_code(char* str, kind* ch, int num)
{
    char c;
    int count = 1;
    for (int i = 0; i < num; i++)
    {
        ch[i].HC = new char[num + 1];
        ch[i].HC[0] = '\0';
    }
    FILE* r;
    r = fopen("cache.txt", "w");
    if (r == NULL)
    {
        printf("Failed to open file\n");
        return;
    }
    fprintf(r, "%s", str);
    fclose(r);
    FILE* file = fopen("cache.txt", "r");
    ch[0].ch = fgetc(file);
    for (int i = 0; count < num + 1; i++)
    {
        c = fgetc(file);
        if (c == '0' || c == '1')
        {
            char str[2] = { c,'\0' };
            ch[count - 1].HC = strcat(ch[count - 1].HC, str);
        }
        else
        {
            ch[count].ch = c;
            count++;
            i = -1;
        }
    }
    if (ch[num - 1].ch == 'O')
    {
        ch[num - 1].ch = '0';
    }
    fclose(file);
    remove("cache.txt");
}

void readBitsToString(FILE* q, char* str)
{
    unsigned char byte;
    int flag = 0, j = 0, newLineCount = 0;
    bool startRead = false;

    // 先遍历文件直到遇到三个连续的换行符
    while (fread(&byte, sizeof(unsigned char), 1, q) == 1)
    {
        if (!startRead)
        {
            if (byte == '\n') // 检测换行符
            {
                newLineCount++;
                if (newLineCount == 3) // 当遇到三个连续的换行符时，开始读取位信息
                {
                    startRead = true;
                    continue; // 继续读取下一个字节
                }
            }
            else
            {
                newLineCount = 0; // 重置连续换行符计数
            }
        }
        else
        {
            // 开始读取位信息
            for (int i = 7; i >= 0; --i)
            {
                str[j] = ((byte >> i) & 1) ? '1' : '0';
                if (str[j] == '1')
                {
                    flag = j;
                }
                j++;
            }
        }
    }
    str[flag] = '\0';
}

void decode(kind* ch, int num, char* encoded_string, char* decoded_string)
{
    char* temp_code = new char[num + 1];
    temp_code[0] = '\0';
    int i, j = 0;
    for (i = 0; encoded_string[i] != '\0'; i++)
    {
        char string[2] = { encoded_string[i],'\0' };
        strcat(temp_code, string);
        for (int k = 0; k < num; k++)
        {
            if (strcmp(temp_code, ch[k].HC) == 0)
            {
                decoded_string[j++] = ch[k].ch;
                temp_code[0] = '\0';
                break;
            }
        }
    }
    decoded_string[j] = '\0';
    delete[] temp_code;
}

bool extractAndRemovePart(const char* filePath, char** outStr) {
    FILE* file = fopen(filePath, "r");
    if (!file) {
        //printf("Failed to open file\n");
        return false;
    }
    char* str = (char*)malloc(MAXSIZE * sizeof(char));
    if (!str)
    {
        fclose(file);
        return false;
    }
    int ch;
    int newLineCount = 0;
    size_t index = 0;
    while ((ch = fgetc(file)) != EOF && newLineCount < 3)
    {
        if (ch == '\n')
        {
            newLineCount++;
        }
        else
        {
            newLineCount = 0;
        }
        if (newLineCount < 3)
        {
            str[index++] = ch;
        }
    }
    str[index - 2] = '\0';
    fclose(file);
    *outStr = str;
    return true;
}

int zip(const char* inputFilePath)
{
    FILE* p;
    char* str;
    kind ch[NUM];
    HuffmanTree HT;
    int num, count = 1;
    for (int i = 0; i < NUM; i++)
    {
        ch[i].count = 0;
    }
    p = fopen(inputFilePath, "r");
    if (p == NULL)
    {
        //printf("Failed to open file\n");
        //printf("Please input the text you want to zip in Zip.txt\n");
        return 1;
    }
    str = new char[MAXSIZE];
    for (int i = 0; i < MAXSIZE; i++)
    {
        str[i] = '\0';
        str[i] = fgetc(p);
        if (str[i] == -1)
        {
            str[i] = '\0';
            break;
        }
    }
    fclose(p);
    //printf("text:%s\n", str);
    num = kinds(str, ch);
    char* encode_str = new char[MAXSIZE * num];
    encode_str[0] = '\0';
    /*printf("ch\tfrequency\n");
    for (int i = 0; i < num; i++)
    {
        printf("%c\t%.6f\n", ch[i].ch, float(ch[i].count) / strlen(str));
    }
    printf("\tnum:%d\n", num);*/
    CreatHuffmanTree(HT, num, ch);
    CreatHuffmanCode(HT, ch, num);
    //printf("ch\tcount\tparent\tlchild\trchild\n");
    //for (int i = 1; i <= 2 * num - 1; i++)
    //{
    //    printf("%c\t", HT[i].weight.ch);
    //    printf("%d\t", HT[i].weight.count);
    //    printf("%d\t", HT[i].parent);
    //    printf("%d\t", HT[i].lchild);
    //    printf("%d\t", HT[i].rchild);
    //    printf("\n");
    //}
    //printf("ch\tCode\n");
    //for (int i = 0; i < num; i++)
    //{
    //    printf("%c\t%s\n", ch[i].ch, ch[i].HC);
    //}
    FILE* r;
    r = fopen("Zip.txt", "w");
    if (r == NULL)
    {
        //printf("Failed to open file\n");
        return 1;
    }
    for (int i = 0; i < num; i++)
    {
        write_huffman_code(r, ch[i]);
    }
    fprintf(r, "%c", char(num));

    fprintf(r, "\n\n\n");

    encode(str, ch, encode_str, num);
    //printf("encode_str:%s\n", encode_str);
    writeBitsToFile(encode_str, r);

    fclose(r);
    //printf("Zip success!\n");
    return 999;
}

int dezip(const char* inputFilePath)
{
    FILE* r;
    kind ch[NUM];
    char* str;
    int count = 1;
    char num;
    str = new char[MAXSIZE];
    extractAndRemovePart(inputFilePath, &str);
    num = str[strlen(str) - 1];
    //printf("num:%d\n", num);
    read_huffman_code(str, ch, num);
    //printf("ch\tCode\n");
   /* for (int i = 0; i < num; i++)
    {
        printf("%c\t%s\n", ch[i].ch, ch[i].HC);
    }
    printf("\tnum:%d\n\n", num);*/

    r = fopen(inputFilePath, "r");
    if (r == NULL)
    {
        return 0;
    }
    char* encode = new char[MAXSIZE * num];
    encode[0] = '\0';
    readBitsToString(r, encode);
    //printf("encode:%s\n", encode);
    char* decode_str = new char[MAXSIZE];
    decode(ch, num, encode, decode_str);
   /* printf("\n\n");
    printf("decode_str:\n\t%s\n", decode_str);*/
    FILE* p;
    p = fopen("Recv.txt", "w");
    if (p == NULL)
    {
        //printf("Failed to open file\n");
        return 0;
    }
    fprintf(p, "%s", decode_str);
    fclose(p);
    fclose(r);
    return 999;
    //printf("Dezip success!\n");
}