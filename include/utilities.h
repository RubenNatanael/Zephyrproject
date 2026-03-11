#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>
#include <string.h>

static int base64_decode(const char *src, unsigned char *dst, size_t dst_len, size_t *out_len)
{
    static const unsigned char base64_table[256] = {
        ['A'] = 0, ['B'] = 1, ['C'] = 2, ['D'] = 3, ['E'] = 4, ['F'] = 5, ['G'] = 6, ['H'] = 7,
        ['I'] = 8, ['J'] = 9, ['K'] = 10, ['L'] = 11, ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
        ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
        ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29, ['e'] = 30, ['f'] = 31,
        ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35, ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39,
        ['o'] = 40, ['p'] = 41, ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
        ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55,
        ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59, ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63,
        ['='] = 0
    };

    size_t i = 0, j = 0;
    size_t len = strlen(src);

    while (i < len && j < dst_len - 1) {
        uint32_t sextet_a = src[i] == '=' ? 0 & i++ : base64_table[(unsigned char)src[i++]];
        uint32_t sextet_b = src[i] == '=' ? 0 & i++ : base64_table[(unsigned char)src[i++]];
        uint32_t sextet_c = src[i] == '=' ? 0 & i++ : base64_table[(unsigned char)src[i++]];
        uint32_t sextet_d = src[i] == '=' ? 0 & i++ : base64_table[(unsigned char)src[i++]];

        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        if (j < dst_len) dst[j++] = (triple >> 16) & 0xFF;
        if (j < dst_len) dst[j++] = (triple >> 8) & 0xFF;
        if (j < dst_len) dst[j++] = triple & 0xFF;
    }

    *out_len = j;
    return 0;
}

#endif