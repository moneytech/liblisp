/** @file       util.c
 *  @brief      utility functions that are used within the liblisp project, but
 *              would also be of use throughout the project.
 *  @author     Richard Howe (2015)
 *  @license    LGPL v2.1 or Later
 *  @email      howe.r.j.89@gmail.com**/

#include "liblisp.h"
#include "private.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int matchhere(regex_result *r, char *regexp, char *text, size_t depth);
static int matchstar(regex_result *r, int literal, int c, char *regexp, char *text, size_t depth);

void pfatal(char *msg, char *file, long line) {
        fprintf(stderr, "(error \"%s\" \"%s\" %ld)\n", msg, file, line);
        exit(-1);
}

char *lstrdup(const char *s) { assert(s);
        char *str;
        if(!(str = malloc(strlen(s) + 1))) 
                return NULL;
        strcpy(str, s);
        return str;
}

int match(char *pat, char *str) { /**@todo add max depth check and other checks*/
        if(!str) return 0;
again:  switch(*pat) {
        case '\0': return !*str;
        case '*':  return match(pat+1, str) || (*str && match(pat, str+1));
        case '.':  if(!*str)        return  0; pat++; str++; goto again;
        case '\\': if(!*(pat+1))    return -1; if(!*str) return 0; pat++; /*fall through*/
        default:   if(*pat != *str) return  0; pat++; str++; goto again;
        }
}

/*This regex engine is a bit of a mess and needs to be rewritten*/
regex_result regex_match(char *regexp, char *text) { 
        regex_result rr = {text, text, 0};
        size_t depth = 0;
        if (regexp[0] == '^')
                return matchhere(&rr, regexp + 1, text, depth + 1), rr;
        do { /* must look even if string is empty */
                rr.start = text;
                if (matchhere(&rr, regexp, text, depth + 1))
                        return rr;
        } while (*text++ != '\0');
        return rr.result = 0, rr;
}

static int matchhere(regex_result *r, char *regexp, char *text, size_t depth) {
        if (REGEX_MAX_DEPTH < depth)
                return r->result = -1;
 BEGIN:
        if (regexp[0] == '\0')
                return r->end = MAX(r->end, text), r->result = 1;
        if (regexp[0] == '\\' && regexp[1] == *text) {
                if (regexp[1] == *text) {
                        regexp += 2;
                        text++;
                        goto BEGIN;
                } else {
                        return r->end = MAX(r->end, text), r->result = -1;
                }
        }
        if (regexp[1] == '?') {
                text = text + (regexp[0] == *text ? 1 : 0);
                regexp = regexp + 2;
                goto BEGIN;
        }
        if (regexp[1] == '+') {
                if (regexp[0] == '.' || regexp[0] == *text)
                        return r->result = matchstar(r, 0, regexp[0], regexp + 2, text, depth + 1);
                r->end = MAX(r->end, text);
                return 0;
        }
        if (regexp[1] == '*')
                return r->end = MAX(r->end, text), 
                       r->result = matchstar(r, 0, regexp[0], regexp + 2, text, depth + 1);
        if (regexp[0] == '$' && regexp[1] == '\0')
                return r->end = MAX(r->end, text), r->result = (*text == '\0' ? 1 : 0);
        if (*text != '\0' && (regexp[0] == '.' || regexp[0] == *text)) {
                regexp++;
                text++;
                goto BEGIN;
        }
        return r->end = MAX(r->end, text), r->result = 0;
}

static int matchstar(regex_result *r, int literal, int c, char *regexp, char *text, size_t depth) {
        if (REGEX_MAX_DEPTH < depth)
                return r->result = -1;
        do { /* a* matches zero or more instances */
                if (matchhere(r, regexp, text, depth + 1))
                        return r->end = MAX(r->end, text), r->result = 1;
        } while (*text != '\0' && (*text++ == c || (c == '.' && !literal)));
        return r->end = MAX(r->end, text), r->result = 0;
}


uint32_t djb2(const char *s, size_t len) { assert(s); 
        uint32_t h = 5381;   /*magic number this hash uses, it just is*/
        size_t i = 0;
        for (i = 0; i < len; s++, i++) h = ((h << 5) + h) + (*s);
        return h;
}

char *getadelim(FILE *in, int delim) { assert(in);
        io io_in;
        memset(&io_in, 0, sizeof(io_in));
        io_in.p.file = in;
        io_in.type = FIN;
        return io_getdelim(&io_in, delim);
}

char *getaline(FILE *in) { assert(in); return getadelim(in, '\n'); }

char *lstrcatend(char *dest, const char *src) { assert(dest && src);
        size_t sz = strlen(dest);
        strcpy(dest + sz, src);
        return dest + sz;
}

char *vstrcatsep(const char *separator, const char *first, ...) { 
        size_t len, seplen, num = 0;
        char *retbuf, *va, *p;
        va_list argp1, argp2;

        if (!separator || !first) return NULL;
        len    = strlen(first);
        seplen = strlen(separator);

        va_start(argp1, first);
        va_copy(argp2, argp1);
        while ((va = va_arg(argp1, char *))) 
                 num++, len += strlen(va);
        va_end(argp1);

        len += (seplen * num);
        if (!(retbuf = malloc(len + 1))) return NULL;
        retbuf[0] = '\0';
        p = lstrcatend(retbuf, first);
        va_start(argp2, first);
        while ((va = va_arg(argp2, char *)))
                 p = lstrcatend(p, separator), p = lstrcatend(p, va);
        va_end(argp2);
        return retbuf;
}

uint8_t binlog(unsigned long long v) { /*binary logarithm*/
        uint8_t r = 0;
        while(v >>= 1) r++;
        return r;
}

uint64_t xorshift128plus(uint64_t s[2]) { /*PRNG*/
	uint64_t x = s[0];
	uint64_t const y = s[1];
	s[0] = y;
	x ^= x << 23; /*a*/
	x ^= x >> 17; /*b*/
	x ^= y ^ (y >> 26); /*c*/
	s[1] = x;
	return x + y;
}

int balance(const char *sexpr) { assert(sexpr);
        int bal = 0, c; 
        while((c = *sexpr++))
                if     (c == '(') bal++;
                else if(c == ')') bal--;
                else if(c == '"')
                        while((c = *sexpr++)) {
                                if (c == '\\' && '"' == *sexpr) sexpr++;
                                else if (c == '"') break;
                                else continue;
                        }
                else continue;
        return bal;
}

