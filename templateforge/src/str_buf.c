#include "str_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void sb_grow(StrBuf* sb, int needed) {
    if (sb->len + needed < sb->cap) return;
    int nc = sb->cap ? sb->cap * 2 : 64;
    while (nc < sb->len + needed + 1) nc *= 2;
    sb->data = realloc(sb->data, nc);
    sb->cap  = nc;
}

void sb_init(StrBuf* sb) { sb->data = NULL; sb->len = sb->cap = 0; }

void sb_free(StrBuf* sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

void sb_appendn(StrBuf* sb, const char* s, int n) {
    if (!s || n <= 0) return;
    sb_grow(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_append(StrBuf* sb, const char* s) {
    if (s) sb_appendn(sb, s, (int)strlen(s));
}

void sb_appendc(StrBuf* sb, char c) {
    sb_grow(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len]   = '\0';
}

void sb_appendf(StrBuf* sb, const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n <= 0) { va_end(ap); return; }
    sb_grow(sb, n);
    vsnprintf(sb->data + sb->len, n + 1, fmt, ap);
    sb->len += n;
    va_end(ap);
}

void sb_reset(StrBuf* sb) {
    sb->len = 0;
    if (sb->data) sb->data[0] = '\0';
}

char* sb_dup(const StrBuf* sb) {
    return strdup(sb->data ? sb->data : "");
}
