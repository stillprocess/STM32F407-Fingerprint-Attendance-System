#ifndef __SHA256_H
#define __SHA256_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint8_t data[64];     // 当前 512 位数据块
    uint32_t datalen;     // 当前数据块有效字节数
    uint64_t bitlen;      // 已处理数据位数
    uint32_t state[8];    // 哈希状态
} SHA256_CTX;

void SHA256_Init(SHA256_CTX *ctx);                                      // 初始化 SHA-256
void SHA256_Update(SHA256_CTX *ctx, const uint8_t *data, size_t len);   // 输入待计算数据
void SHA256_Final(SHA256_CTX *ctx, uint8_t hash[32]);                   // 输出 32 字节摘要
void Password_Hash(const char *password, uint8_t hash[32]);             // 计算密码摘要

#endif
