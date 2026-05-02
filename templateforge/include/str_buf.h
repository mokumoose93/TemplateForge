#pragma once
#include <stdarg.h>
#include <stddef.h>

/* Dynamic string buffer used throughout the codebase in place of std::string /
   std::ostringstream.  All functions accept NULL gracefully. */
typedef struct {
    char*  data;
    int    len;
    int    cap;
} StrBuf;

void  sb_init   (StrBuf* sb);
void  sb_free   (StrBuf* sb);
void  sb_append (StrBuf* sb, const char* s);
void  sb_appendn(StrBuf* sb, const char* s, int n);
void  sb_appendc(StrBuf* sb, char c);
void  sb_appendf(StrBuf* sb, const char* fmt, ...);
void  sb_reset  (StrBuf* sb);
/* Returns heap-allocated copy of current content.  Caller must free(). */
char* sb_dup    (const StrBuf* sb);
