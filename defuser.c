#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[8];
    size_t count = 0;

    uint32_t low  = (uint32_t)challenge;
    uint32_t high = (uint32_t)(challenge >> 32);
    uint32_t mid  = (uint32_t)(challenge >> 16);

    uint32_t step2  = (high + low * 8) ^ 0x448c;
    uint32_t step0  = (mid + 0xd4f6) ^ low;
    uint32_t answer = 3 * (step0 - step2) + 0x37c9;

    values[count++] = answer;

    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[8];
    size_t count = 0;

    uint32_t v0 = (uint32_t)((challenge >> 41) & 0xFF) ^ 7;
    values[count++] = v0;

    for (int i = 1; i <= 5; i++) {
        uint32_t shift = 19 + 3 * (i - 1);
        uint32_t bits = (uint32_t)((challenge >> shift) & 0xF);
        uint32_t v = (bits ^ (uint32_t)i) + 2 * values[i - 1];
        values[count++] = v;
    }

    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[8];
    size_t count = 0;

    uint32_t v0 = (uint32_t)(challenge & 7);
    uint32_t X = (uint32_t)(challenge >> 32) ^ (uint32_t)(challenge >> 3);

    uint32_t v1;
    switch (v0) {
        case 0:  // p3_case_4
            v1 = (2 * X) ^ 0xbbe1;
            break;
        case 1:  // p3_case_1
            v1 = (4 * X) ^ 0x9ed7;
            break;
        case 2:  // p3_case_7
            v1 = (X ^ 0x5044) + (X >> 2);
            break;
        case 3:  // p3_case_6
            v1 = 5 * X + 0x2855;
            break;
        case 4:  // p3_case_5
            v1 = 5 * X + 0xc243;
            break;
        case 5:  // p3_case_3
            v1 = (X ^ 0xee13) + (X >> 1);
            break;
        case 6:  // p3_case_2
            v1 = 3 * X + 0x1931;
            break;
        case 7:  // p3_case_0
            v1 = 5 * X + 0x3d20c;
            break;
        default:
            v1 = 0;
            break;
    }

    values[count++] = v0;
    values[count++] = v1;

    return write_answer(out, cap, values, count);
}

static uint32_t p4_walk(uint32_t target, uint32_t lo, uint32_t hi, uint32_t key) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (mid == target) {
        return (mid + 0x16) ^ key;
    }
    uint32_t ebx = mid + key;
    if (target < mid) {
        uint32_t r = p4_walk(target, lo, mid - 1, key);
        return (r * 5) ^ ebx ^ 0x15;
    } else {
        uint32_t r = p4_walk(target, mid + 1, hi, key);
        return (r * 5) ^ ebx ^ 0xb;
    }
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[8];
    size_t count = 0;

    uint32_t target = (uint32_t)challenge & 0x7f;
    uint32_t key    = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
    uint32_t answer = p4_walk(target, 0, 0x7f, key);

    values[count++] = answer;
    return write_answer(out, cap, values, count);
}
typedef struct {
    uint32_t field0;
    uint32_t field1;
    int      next;      // -1 表示 NULL
} P5Node;

static const P5Node p5_nodes[] = {
    { 0x46dd, 0x164,  5 },  /* 0  */
    { 0xb916, 0x826,  4 },  /* 1  */
    { 0xe72a, 0x529,  8 },  /* 2  */
    { 0x653f, 0x27e,  7 },  /* 3  */
    { 0x1367, 0x414,  9 },  /* 4  */
    { 0x2d23, 0x331, 11 },  /* 5  */
    { 0x9d47, 0xa6d,  2 },  /* 6  */
    { 0x186e, 0x022,  1 },  /* 7  */
    { 0x3720, 0xb17, 10 },  /* 8  */
    { 0x85ad, 0x767,  0 },  /* 9  */
    { 0x5c9c, 0x956, -1 },  /* 10 */
    { 0xddfe, 0x630,  6 },  /* 11 */
};

static const int p5_heads[8] = { 3, 7, 1, 4, 9, 0, 5, 11 };

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[8];
    size_t count = 0;

    uint32_t idx   = (uint32_t)challenge & 7;
    uint32_t steps = ((uint32_t)challenge >> 3) & 3;

    int node = p5_heads[idx];

    uint32_t eax = ((uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32)) ^ 0x14b7;
    eax += p5_nodes[node].field0;
    eax *= 5;
    uint32_t last_field1 = p5_nodes[node].field1;
    eax ^= last_field1;

    for (uint32_t i = 0; i < steps; i++) {
        node = p5_nodes[node].next;
        eax += p5_nodes[node].field0;
        eax *= 5;
        last_field1 = p5_nodes[node].field1;
        eax ^= last_field1;
    }

    values[count++] = eax;           // 第 1 个输入：返回值
    values[count++] = last_field1;   // 第 2 个输入：写入的 field1

    return write_answer(out, cap, values, count);
}

static uint32_t p6_op_0(uint32_t x) { return ((x >> 7) ^ x) + 0x49c2; }
static uint32_t p6_op_1(uint32_t x) { return (x ^ 0x571d) + (x << 7); }
static uint32_t p6_op_2(uint32_t x) { return (8u * x + 0x94c3) ^ (3u * x); }
static uint32_t p6_op_3(uint32_t x) { return ((x << 1) | (x >> 31)) ^ 0x88b3; }
static uint32_t p6_op_4(uint32_t x) { return (x >> 1) | (x << 31); }
static uint32_t p6_op_5(uint32_t x) { return (x + 0xa8e8) ^ (x >> 5); }

typedef uint32_t (*p6_fn)(uint32_t);
static const p6_fn p6_table[8] = {
    p6_op_0, p6_op_0, p6_op_4, p6_op_3,
    p6_op_4, p6_op_5, p6_op_1, p6_op_2,
};

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[8];
    size_t count = 0;

    uint32_t idx1 = (uint32_t)challenge & 7;
    uint32_t arg1 = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
    uint32_t r1   = p6_table[idx1](arg1);

    uint32_t idx2 = ((uint32_t)challenge >> 3) & 7;
    uint32_t arg2 = r1 ^ 0xb2c1;
    uint32_t r2   = p6_table[idx2](arg2);

    uint32_t answer = r2 + 0xdba7;

    values[count++] = answer;
    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    uint32_t a = (uint32_t)(context & 3);
    uint32_t X = (uint32_t)(challenge >> 32) ^ (uint32_t)challenge;
    uint32_t Y = (uint32_t)(context >> 32) ^ (uint32_t)context;
    uint32_t Z = (uint32_t)(0xb07a * a) ^ (uint32_t)(context >> 16);

    uint32_t t   = X ^ Y;
    uint32_t mix = ((t << 1) | (t >> 31)) + 0xb07a;
    mix ^= 0x3b81;

    uint32_t b = mix ^ Z;
    uint32_t c = b ^ (0x3b81 + Y + a + (X >> 1));

    /* 前缀：32 个 'X' + "OPEN"，共 36 字节 */
    char prefix[37];
    for (int i = 0; i < 32; i++) prefix[i] = 'X';
    prefix[32] = 'O';
    prefix[33] = 'P';
    prefix[34] = 'E';
    prefix[35] = 'N';
    prefix[36] = '\0';

    int n = snprintf(out, cap, "%s%u %u %u", prefix, a, b, c);
    if (n < 0 || (size_t)n >= cap) return 1;
    return 0;
}