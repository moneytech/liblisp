#include "liblisp.h"
#include "private.h"
#include <locale.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

/*X-Macro of primitive functions and their names; basic built in subr*/
#define SUBROUTINE_XLIST\
        X(subr_band,    "&")              X(subr_bor,       "|")\
        X(subr_bxor,    "^")              X(subr_binv,      "~")\
        X(subr_sum,     "+")              X(subr_sub,       "-")\
        X(subr_prod,    "*")              X(subr_mod,       "%")\
        X(subr_div,     "/")              X(subr_eq,        "=")\
        X(subr_eq,      "eq")             X(subr_greater,   ">")\
        X(subr_less,    "<")              X(subr_cons,      "cons")\
        X(subr_car,     "car")            X(subr_cdr,       "cdr")\
        X(subr_list,    "list")           X(subr_match,     "match")\
        X(subr_scons,   "scons")          X(subr_scar,      "scar")\
        X(subr_scdr,    "scdr")           X(subr_eval,      "eval")\
        X(subr_trace,   "trace-level!")   X(subr_gc,        "gc")\
        X(subr_length,  "length")         X(subr_typeof,    "type-of")\
        X(subr_inp,     "input?")         X(subr_outp,      "output?")\
        X(subr_eofp,    "eof?")           X(subr_flush,     "flush")\
        X(subr_tell,    "tell")           X(subr_seek,      "seek")\
        X(subr_close,   "close")          X(subr_open,      "open")\
        X(subr_getchar, "get-char")       X(subr_getdelim,  "get-delim")\
        X(subr_read,    "read")           X(subr_puts,      "put")\
        X(subr_putchar, "put-char")       X(subr_print,     "print")\
        X(subr_ferror,  "ferror")         X(subr_system,    "system")\
        X(subr_remove,  "remove")         X(subr_rename,    "rename")\
        X(subr_hlookup, "hash-lookup")    X(subr_hinsert,   "hash-insert")\
        X(subr_coerce,  "coerce")         X(subr_time,      "time")\
        X(subr_getenv,  "getenv")         X(subr_rand,      "random")\
        X(subr_seed,    "seed")           X(subr_date,      "date")\
        X(subr_assoc,   "assoc")          X(subr_setlocale, "locale!")\
        X(subr_trace_cell, "trace")       X(subr_binlog,    "binary-logarithm")\
        X(subr_eval_time, "timed-eval")   X(subr_reverse,   "reverse")\
        X(subr_join,    "join")           X(subr_regexspan, "regex-span")\
        X(subr_raise,   "raise")          X(subr_split,     "split")\
        X(subr_hcreate, "hash-create")    X(subr_format,    "format")

#define X(SUBR, NAME) static cell* SUBR (lisp *l, cell *args);
SUBROUTINE_XLIST /*function prototypes for all of the built-in subroutines*/
#undef X

#define X(SUBR, NAME) { SUBR, NAME },
static struct subr_list { subr p; char *name; } primitives[] = {
        SUBROUTINE_XLIST /*all of the subr functions*/
        {NULL, NULL} /*must be terminated with NULLs*/
};
#undef X

#define INTEGER_XLIST\
        X("*seek-cur*",     SEEK_CUR)     X("*seek-set*",    SEEK_SET)\
        X("*seek-end*",     SEEK_END)     X("*random-max*",  INTPTR_MAX)\
        X("*integer-max*",  INTPTR_MAX)   X("*integer-min*", INTPTR_MIN)\
        X("*integer*",      INTEGER)      X("*symbol*",      SYMBOL)\
        X("*cons*",         CONS)         X("*string*",      STRING)\
        X("*hash*",         HASH)         X("*io*",          IO)\
        X("*float*",        FLOAT)        X("*procedure*",   PROC)\
        X("*primitive*",    SUBR)         X("*f-procedure*", FPROC)\
        X("*file-in*",      FIN)          X("*file-out*",    FOUT)\
        X("*string-in*",    SIN)          X("*string-out*",  SOUT)\
        X("*lc-all*",       LC_ALL)       X("*lc-collate*",  LC_COLLATE)\
        X("*lc-ctype*",     LC_CTYPE)     X("*lc-monetary*", LC_MONETARY)\
        X("*lc-numeric*",   LC_NUMERIC)   X("*lc-time*",     LC_TIME)\
        X("*user-defined*", USERDEF)      X("*trace-off*",   TRACE_OFF)\
        X("*trace-marked*", TRACE_MARKED) X("*trace-all*",   TRACE_ALL)\
        X("*gc-on*",        GC_ON)        X("*gc-postpone*", GC_POSTPONE)\
        X("*gc-off*",       GC_OFF)       X("*eof*",         EOF)\
        X("*sig-abrt*",     SIGABRT)      X("*sig-fpe*",     SIGFPE)\
        X("*sig-ill*",      SIGILL)       X("*sig-int*",     SIGINT)\
        X("*sig-segv*",     SIGSEGV)      X("*sig-term*",    SIGTERM)

#define X(NAME, VAL) { NAME, VAL }, 
static struct integer_list { char *name; intptr_t val; } integers[] = {
        INTEGER_XLIST
        {NULL, 0}
};
#undef X

#define X(CNAME, LNAME) static cell _ ## CNAME = { SYMBOL, 0, 0, 1, 0, 0, 0, .p[0].v = LNAME};
CELL_XLIST /*structs for special cells*/
#undef X

#define X(CNAME, NOT_USED) static cell* CNAME = & _ ## CNAME;
CELL_XLIST /*pointers to structs for special cells*/
#undef X

#define X(CNAME, NOT_USED) { & _ ## CNAME },
/**@brief a list of all the special symbols**/
static struct special_cell_list { cell *internal; } special_cells[] = {
        CELL_XLIST 
        { NULL }
};
#undef X

cell *mkerror(void)          { return Error; }
cell *mknil(void)            { return Nil; }
cell *mktee(void)            { return Tee; }
cell *mkquote(void)          { return Quote; }

lisp *lisp_init(void) {
        lisp *l;
        unsigned i;
        if(!(l = calloc(1, sizeof(*l))))       goto fail;
        if(!(l->ifp = io_fin(stdin)))          goto fail;
        if(!(l->ofp = io_fout(stdout)))        goto fail;
        if(!(l->efp = io_fout(stderr)))        goto fail;
        if(!(l->buf = calloc(DEFAULT_LEN, 1))) goto fail;
        l->buf_allocated = DEFAULT_LEN;
        if(!(l->gc_stack = calloc(DEFAULT_LEN, sizeof(*l->gc_stack)))) 
                goto fail;
        l->gc_stack_allocated = DEFAULT_LEN;
        l->max_depth          = LARGE_DEFAULT_LEN; /**< max recursion depth*/

#define X(CNAME, LNAME) l-> CNAME = CNAME;
CELL_XLIST
#undef X

        l->random_state[0] = 0xCAFE; /*Are these good seeds?*/
        l->random_state[1] = 0xBABE; 
        for(i = 0; i < DEFAULT_LEN; i++) /*discard first N numbers*/
                (void)xorshift128plus(l->random_state);

        if(!(l->all_symbols = mkhash(l, hash_create(LARGE_DEFAULT_LEN)))) goto fail;
        if(!(l->top_env = cons(l, cons(l, mknil(), mknil()), mknil()))) goto fail;

        /*it would be nice if this was all statically allocated*/
        for(i = 0; special_cells[i].internal; i++) /*add special cells*/
                if(!lisp_intern(l, special_cells[i].internal))
                        goto fail;

        if(!extend_top(l, mktee(), mktee())) goto fail;

        if(!lisp_add_cell(l, "pi", mkfloat(l, 3.14159265358979323846))) goto fail;
        if(!lisp_add_cell(l, "e",  mkfloat(l, 2.71828182845904523536))) goto fail;

        if(!lisp_add_cell(l, "*stdin*",  mkio(l, io_fin(stdin))))   goto fail;
        if(!lisp_add_cell(l, "*stdout*", mkio(l, io_fout(stdout)))) goto fail;
        if(!lisp_add_cell(l, "*stderr*", mkio(l, io_fout(stderr)))) goto fail;

        for(i = 0; integers[i].name; i++) /*add all integers*/
                if(!lisp_add_cell(l, integers[i].name, mkint(l, integers[i].val)))
                        goto fail;
        for(i = 0; primitives[i].p; i++) /*add all primitives*/
                if(!lisp_add_subr(l, primitives[i].name, primitives[i].p))
                        goto fail;

        return l;
fail:   lisp_destroy(l);
        return NULL;
}

static cell *subr_band(lisp *l, cell *args) {
        if(!cklen(args, 2) || !isint(car(args)) || !isint(CADR(args)))
                RECOVER(l, "\"expected (int int)\" '%S", args);
        return mkint(l, intval(car(args)) & intval(CADR(args)));
}

static cell *subr_bor(lisp *l, cell *args) {
        if(!cklen(args, 2) || !isint(car(args)) || !isint(CADR(args)))
                RECOVER(l, "\"expected (int int)\" '%S", args);
        return mkint(l, intval(car(args)) | intval(CADR(args)));
}

static cell *subr_bxor(lisp *l, cell *args) {
        if(!cklen(args, 2) || !isint(car(args)) || !isint(CADR(args)))
                RECOVER(l, "\"expected (int int)\" '%S", args);
        return mkint(l, intval(car(args)) ^ intval(CADR(args)));
}

static cell *subr_binv(lisp *l, cell *args) {
        if(!cklen(args, 1) || !isint(car(args)))
                RECOVER(l, "\"expected (int)\" '%S", args);
        return mkint(l, ~intval(car(args)));
}

static cell *subr_binlog(lisp *l, cell *args) {
        if(!cklen(args, 1) || !isint(car(args)))
                RECOVER(l, "\"expected (int)\" '%S", args);
        return mkint(l, binlog(intval(car(args))));
}

static cell *subr_sum(lisp *l, cell *args) {
        if(!cklen(args, 2)) 
                RECOVER(l, "\"argument count not equal 2\" '%S", args);
        cell *x = car(args), *y = CADR(args);
        if(isint(x) && isarith(y)) {
                if(isfloat(y)) return mkint(l, intval(x) + floatval(y));
                else           return mkint(l, intval(x) + intval(y));
        } else if(isfloat(x) && isarith(y)) {
                if(isfloat(y)) return mkfloat(l, floatval(x) + floatval(y));
                else           return mkfloat(l, floatval(x) + (lfloat) intval(y));
        }
        RECOVER(l, "\"type check problem\" %S", args);
        return mkerror();
}

static cell *subr_sub(lisp *l, cell *args) {
        if(!cklen(args, 2)) 
                RECOVER(l, "\"argument count not equal 2\" '%S", args);
        cell *x = car(args), *y = CADR(args);
        if(isint(x) && isarith(y)) {
                if(isfloat(y)) return mkint(l, intval(x) - floatval(y));
                else           return mkint(l, intval(x) - intval(y));
        } else if(isfloat(x) && isarith(y)) {
                if(isfloat(y)) return mkfloat(l, floatval(x) - floatval(y));
                else           return mkfloat(l, floatval(x) - (lfloat) intval(y));
        }
        RECOVER(l, "\"type check failed\" '%S", args);
        return mkerror(); 
}

static cell *subr_prod(lisp *l, cell *args) {
        if(!cklen(args, 2)) 
                RECOVER(l, "\"argument count not equal 2\" '%S", args);
        cell *x = car(args), *y = CADR(args);
        if(isint(x) && isarith(y)) {
                if(isfloat(y)) return mkint(l, intval(x) * floatval(y));
                else           return mkint(l, intval(x) * intval(y));
        } else if(isfloat(x) && isarith(y)) {
                if(isfloat(y)) return mkfloat(l, floatval(x) * floatval(y));
                else           return mkfloat(l, floatval(x) * (lfloat) intval(y));
        }
        RECOVER(l, "\"type check failed\" '%S", args);
        return mkerror();
}

static cell *subr_mod(lisp *l, cell *args) {
        intptr_t dividend, divisor;
        if(!cklen(args, 2) || !isint(car(args)) || !isint(CADR(args)))
                RECOVER(l, "\"argument count not equal 2\" '%S", args);
        dividend = intval(car(args));
        divisor  = intval(CADR(args));
        if(!divisor || (dividend == INTPTR_MIN && divisor == -1)) 
                RECOVER(l, "\"invalid divisor values\" '%S", args);
        return mkint(l, dividend % divisor);
}

static cell *subr_div(lisp *l, cell *args) {
        if(!cklen(args, 2))
                RECOVER(l, "\"argument count not equal 2\" '%S", args);
        if(isint(car(args)) && isarith(CADR(args))) {
                intptr_t dividend, divisor;
                dividend = intval(car(args));
                divisor = isfloat(CADR(args)) ? 
                        floatval(CADR(args)) : 
                        intval(CADR(args));
                if(!divisor || (dividend == INTPTR_MIN && divisor == -1))
                        RECOVER(l, "\"invalid divisor values\" '%S", args);
                return mkint(l, dividend / divisor);
        } else if(isfloat(car(args)) && isarith(CADR(args))) {
                lfloat dividend, divisor;
                dividend = floatval(car(args));
                divisor = isfloat(CADR(args)) ? 
                        floatval(CADR(args)) : 
                        intval(CADR(args));
                if(divisor == 0.f)
                        RECOVER(l, "\"division by zero in %S\"", args);
                return mkfloat(l, dividend / divisor);
        }
        RECOVER(l, "\"type check failed\" '%S", args);
        return mkerror();
}

static cell *subr_greater(lisp *l, cell *args) {
        cell *x, *y;
        if(!cklen(args, 2))
                RECOVER(l, "\"expected (number number) or (string string)\" '%S", args);
        x = car(args);
        y = CADR(args);
        if(isarith(x) && isarith(y))
                return  (isfloat(x) ? floatval(x) : intval(x)) > 
                        (isfloat(y) ? floatval(y) : intval(y)) ? mktee() : mknil();
        else if(isasciiz(x) && isasciiz(x))
                return (strcmp(strval(x), strval(y)) > 0) ? mktee() : mknil();
        RECOVER(l, "\"expected (number number) or (string string)\" '%S", args);
        return mkerror();
}

static cell *subr_less(lisp *l, cell *args) {
        cell *x, *y;
        if(!cklen(args, 2))
                RECOVER(l, "\"expected (number number) or (string string)\" '%S", args);
        x = car(args);
        y = CADR(args);
        if(isarith(x) && isarith(y))
                return  (isfloat(x) ? floatval(x) : intval(x)) < 
                        (isfloat(y) ? floatval(y) : intval(y)) ? mktee() : mknil();
        else if(isasciiz(x) && isasciiz(x))
                return (strcmp(strval(x), strval(y)) < 0) ? mktee() : mknil();
        RECOVER(l, "\"expected (number number) or (string string)\" '%S", args);
        return mkerror();
}

static cell *subr_eq(lisp *l, cell *args) {
        cell *x, *y;
        if(!cklen(args, 2))
                RECOVER(l, "'arg-count \"argc != 2 in %S\"", args);
        x = car(args);
        y = CADR(args);
        if(isuserdef(x) && l->ufuncs[x->userdef].equal)
                return (l->ufuncs[x->userdef].equal)(x, y) ? mktee() : mknil();
        if(intval(x) == intval(y))
                return mktee();
        if(isstr(x) && isstr(y)) {
                if(!strcmp(strval(x), strval(y))) return mktee();
                else return mknil();
        }
        return mknil();
}

static cell *subr_cons(lisp *l, cell *args) { 
        if(!cklen(args, 2))
                RECOVER(l, "\"expected (expr expr)\" '%S", args);
        return cons(l, car(args), CADR(args)); 
}

static cell *subr_car(lisp *l, cell *args) { 
        if(!cklen(args, 1) || !iscons(car(args)))
                RECOVER(l, "\"expect (list)\" '%S", args);
        return CAAR(args); 
}

static cell *subr_cdr(lisp *l, cell *args) { 
        if(!cklen(args, 1) || !iscons(car(args)))
                RECOVER(l, "\"argument count not equal 1 or not a list\" '%S", args);
        return CDAR(args); 
}

static cell *subr_list(lisp *l, cell *args) {
        size_t i;
        cell *op, *head;
        if(cklen(args, 0))
                RECOVER(l, "\"argument count must be more than 0\" '%S", args);
        op = car(args);
        args = cdr(args);
        head = op = cons(l, op, mknil());
        for(i = 1; !isnil(args); args = cdr(args), op = cdr(op), i++)
                setcdr(op, cons(l, car(args), mknil()));
        head->len = i;
        return head;
}

static cell *subr_match(lisp *l, cell *args) { 
        if(!cklen(args, 2) 
        || !isasciiz(car(args)) || !isasciiz(CADR(args))) 
                RECOVER(l, "\"expected (string string)\" '%S", args);
        return match(symval(car(args)), symval(CADR(args))) ? mktee() : mknil(); 
}

static cell *subr_scons(lisp *l, cell *args) {
        char *ret;
        if(!cklen(args, 2) 
        || !isasciiz(car(args)) || !isasciiz(CADR(args)))
                RECOVER(l, "\"expected (string string)\" '%S", args);
        ret = CONCATENATE(strval(car(args)), strval(CADR(args)));
        return mkstr(l, ret);
}

static cell *subr_scar(lisp *l, cell *args) {
        char c[2] = {'\0', '\0'};
        if(!cklen(args, 1) || !isasciiz(car(args)))
                RECOVER(l, "\"expected (string-or-symbol)\" '%S", args);
        c[0] = strval(car(args))[0];
        return mkstr(l, lstrdup(c));
}


static cell *subr_scdr(lisp *l, cell *args) {
        if(!cklen(args, 1) || !isasciiz(car(args)))
                RECOVER(l, "\"expected (string-or-symbol)\" '%S", args);
        if(!(strval(car(args))[0])) mkstr(l, lstrdup(""));
        return mkstr(l, lstrdup(&strval(car(args))[1]));;
}

static cell *subr_eval(lisp *l, cell *args) { /**@bug allows unlimited recursion!**/
        cell *ob = NULL;
        int restore_used, r;
        jmp_buf restore;
        if(l->recover_init) {
                memcpy(restore, l->recover, sizeof(jmp_buf));
                restore_used = 1;
        }
        l->recover_init = 1;
        if((r = setjmp(l->recover))) {
                RECOVER_RESTORE(restore_used, l, restore); 
                return mkerror();
        }

        if(cklen(args, 1)) ob = eval(l, 0, car(args), l->top_env);
        if(cklen(args, 2)) {
                if(!iscons(CADR(args)))
                        RECOVER(l, "\"expected a-list\" '%S", args);
                ob = eval(l, 0, car(args), CADR(args));
        }

        RECOVER_RESTORE(restore_used, l, restore); 
        if(!ob) RECOVER(l, "\"expected (expr) or (expr environment)\" '%S", args);
        return ob;
}

static cell *subr_trace(lisp *l, cell *args) {
        if(cklen(args, 1)) {
                if(isint(car(args))) {
                        switch(intval(car(args))) {
                        case TRACE_OFF:    l->trace = TRACE_OFF; break;
                        case TRACE_MARKED: l->trace = TRACE_MARKED; break;
                        case TRACE_ALL:    l->trace = TRACE_ALL; break;
                        default: RECOVER(l, "\"invalid trace level\" '%S", car(args));
                        }
                } 
                else RECOVER(l, "\"expected (int)\" '%S", args);
        }
        return mkint(l, l->trace);
}

static cell *subr_trace_cell(lisp *l, cell *args) {
        if(cklen(args, 1)) {
                return (car(args)->trace) ? mktee() : mknil();
        } else if (cklen(args, 2)) {
                if(isnil(CADR(args))) {
                        car(args)->trace = 0;
                        return mknil();
                } else if(CADR(args) == mktee()) {
                        car(args)->trace = 1;
                        return mktee();
                } 
        } 
        RECOVER(l, "\"expected (cell) or (cell t-or-nil)\", '%S", args);
        return mkerror();
}

static cell *subr_gc(lisp *l, cell *args) {
        if(cklen(args, 0))
                gc_mark_and_sweep(l);
        if(cklen(args, 1) && isint(car(args))) {
                switch(intval(car(args))) {
                case GC_ON:       if(l->gc_state == GC_OFF) goto fail;
                                  else l->gc_state = GC_ON;
                                  break;
                case GC_POSTPONE: if(l->gc_state == GC_OFF) goto fail;
                                  else l->gc_state = GC_POSTPONE; 
                                  break;
                case GC_OFF:      l->gc_state = GC_OFF;      break;
                default: RECOVER(l, "\"invalid GC option\" '%S", args);
                }
        }
        return mkint(l, l->gc_state);
fail:   RECOVER(l, "\"garbage collection permanently off\" '%S", args);
        return mkerror();
}

static cell *subr_length(lisp *l, cell *args) {
        if(!cklen(args, 1)) RECOVER(l, "\"argument count is not 1\" '%S", args);
        return mkint(l, (intptr_t)car(args)->len);
}

static cell* subr_inp(lisp *l, cell *args) {
        if(!cklen(args, 1)) RECOVER(l, "\"argument count is not 1\" '%S", args);
        return isin(car(args)) ? mktee() : mknil();
}

static cell* subr_outp(lisp *l, cell *args) {
        if(!cklen(args, 1)) RECOVER(l, "\"argument count is not 1\" '%S", args);
        return isout(car(args)) ? mktee() : mknil();
}

static cell* subr_open(lisp *l, cell *args) {
        io *ret = NULL;
        char *file;
        if(!cklen(args, 2) || !isint(car(args)) || !isasciiz(CADR(args))) 
                RECOVER(l, "\"expected (integer string)\" '%S", args);
        file = strval(CADR(args));
        switch(intval(car(args))) {
        case FIN:  ret = io_fin(fopen(file, "rb")); break;
        case FOUT: ret = io_fout(fopen(file, "wb")); break;
        case SIN:  ret = io_sin(file); break;
        /*case SOUT: will not be implemented.*/
        default:   RECOVER(l, "\"invalid operation %d\" '%S", intval(car(args)), args);
        }
        return ret == NULL ? mknil() : mkio(l, ret);
}

static cell* subr_getchar(lisp *l, cell *args) {

        if(cklen(args, 0)) return mkint(l, io_getc(l->ifp));
        if(cklen(args, 1) && isin(car(args)))
                return mkint(l, io_getc(ioval(car(args))));
        RECOVER(l, "\"expected () or (input)\" '%S", args);
        return mkerror();
}

static cell* subr_getdelim(lisp *l, cell *args) {
        int ch;
        char *s; 
        if(cklen(args, 1) && (isasciiz(car(args)) || isint(car(args)))) {
                ch = isasciiz(car(args)) ? strval(car(args))[0] : intval(car(args));
                return (s = io_getdelim(l->ifp, ch)) ? mkstr(l, s) : mknil();
        }
        if(cklen(args, 2) && isin(car(args)) && (isasciiz(CADR(args)) || isint(CADR(args)))) {
                ch = isasciiz(CADR(args)) ? strval(CADR(args))[0] : intval(CADR(args));
                return (s = io_getdelim(ioval(car(args)), ch)) ? mkstr(l, s) : mknil();
        }
        RECOVER(l, "\"expected (string) or (input string)\" '%S", args);
        return mkerror();
}

static cell* subr_read(lisp *l, cell *args) {
        cell *ob = NULL;
        int restore_used, r;
        jmp_buf restore;
        if(l->recover_init) {
                memcpy(restore, l->recover, sizeof(jmp_buf));
                restore_used = 1;
        }
        l->recover_init = 1;
        if((r = setjmp(l->recover))) { 
                RECOVER_RESTORE(restore_used, l, restore); 
                return mkerror();
        }

        if(cklen(args, 0))
                ob = (ob = reader(l, l->ifp)) ? ob : mkerror();
        if(cklen(args, 1) && isin(car(args)))
                ob = (ob = reader(l, ioval(car(args)))) ? ob : mkerror();
        RECOVER_RESTORE(restore_used, l, restore); 
        if(!ob) RECOVER(l, "\"expected () or (input)\" '%S", args);
        return ob;
}

static cell* subr_puts(lisp *l, cell *args) {
        if(cklen(args, 1) && isasciiz(car(args)))
                return io_puts(strval(car(args)),l->ofp) < 0 ? mknil() : car(args);
        if(cklen(args, 2) && isout(car(args)) && isasciiz(CADR(args)))
                return io_puts(strval(CADR(args)), ioval(car(args))) < 0 ?
                        mknil() : CADR(args);
        RECOVER(l, "\"expected (string) or (output string)\" '%S", args);
        return mkerror();
}

static cell* subr_putchar(lisp *l, cell *args) {
        if(cklen(args, 1) && isint(car(args)))
                return io_putc(intval(car(args)),l->ofp) < 0 ? mknil() : car(args);
        if(cklen(args, 2) && isout(car(args)) && isint(CADR(args)))
                return io_putc(intval(car(args)), ioval(CADR(args))) < 0 ?
                        mknil() : CADR(args);
        RECOVER(l, "\"expected (integer) or (output integer)\" '%S", args);
        return mkerror();
}

static cell* subr_print(lisp *l, cell *args) {
        if(cklen(args, 1)) 
                return printer(l, l->ofp, car(args), 0) < 0 ? mknil() : car(args); 
        if(cklen(args, 2) && isout(car(args))) 
                return printer(l, ioval(car(args)), CADR(args), 0) < 0 ? 
                        mknil() : CADR(args); 
        RECOVER(l, "\"expected (expr) or (output expression)\" '%S", args);
        return mkerror();
}

static cell* subr_flush(lisp *l, cell *args) {
        if(cklen(args, 0)) return mkint(l, fflush(NULL));
        if(cklen(args, 1) && isio(car(args))) 
                return io_flush(ioval(car(args))) ? mknil() : mktee();
        RECOVER(l, "\"expected () or (io)\" '%S", args);
        return mkerror();
}

static cell* subr_tell(lisp *l, cell *args) {
        if(cklen(args, 1) && isio(car(args)))
                return mkint(l, io_tell(ioval(car(args))));
        RECOVER(l, "\"expected (io)\" '%S", args);
        return mkerror();
}

static cell* subr_seek(lisp *l, cell *args) { 
        if(cklen(args, 3) && isio(car(args)) 
                && isint(CADR(args)) && isint(CADR(cdr(args)))) {
                switch (intval(CADR(cdr(args)))) {
                case SEEK_SET: case SEEK_CUR: case SEEK_END: break;
                default: RECOVER(l, "\"invalid enum option\" '%S", args);
                }
                return mkint(l,io_seek(ioval(car(args)),intval(CADR(args)),
                                        intval(CADR(cdr(args)))));
        }
        RECOVER(l, "\"expected (io integer integer)\" '%S", args);
        return mkerror();
}

static cell* subr_eofp(lisp *l, cell *args) {
        if(cklen(args, 1) && isio(car(args)))
                return io_eof(ioval(car(args))) ? mktee() : mknil();
        RECOVER(l, "\"expected (io)\" '%S", args);
        return mkerror();
}

static cell* subr_ferror(lisp *l, cell *args) {
        if(cklen(args, 1) && isio(car(args)))
                return io_error(ioval(car(args))) ? mktee() : mknil();
        RECOVER(l, "\"expected (io)\" '%S", args);
        return mkerror();
}

static cell* subr_system(lisp *l, cell *args) {
        if(cklen(args, 0))
                return mkint(l, system(NULL));
        if(cklen(args, 1) && isasciiz(car(args)))
                return mkint(l, system(strval(car(args))));
        RECOVER(l, "\"expected () or (string)\" '%S", args);
        return mkerror();
}

static cell* subr_remove(lisp *l, cell *args) {
        if(!cklen(args, 1) || !isasciiz(car(args)))
                RECOVER(l, "\"expected (string)\" '%S", args);
        return remove(strval(car(args))) ? mknil() : mktee() ;
}

static cell* subr_rename(lisp *l, cell *args) {
        if(!cklen(args, 2) 
        || !isasciiz(car(args)) || !isasciiz(CADR(args))) 
                RECOVER(l, "\"expected (string string)\" '%S", args);
        return rename(strval(car(args)), strval(CADR(args))) ? mknil() : mktee();
}

static cell* subr_hlookup(lisp *l, cell *args) {
        cell *ob;
        if(!cklen(args, 2) || !ishash(car(args)) || !isasciiz(CADR(args)))
                RECOVER(l, "\"expected (hash symbol-or-string)\" %S", args);
        return (ob = hash_lookup(hashval(car(args)),
                                symval(CADR(args)))) ? ob : mknil(); 
}

static cell* subr_hinsert(lisp *l, cell *args) {
        if(!cklen(args, 3) || !ishash(car(args)) || !isasciiz(CADR(args)))
                RECOVER(l, "\"expected (hash symbol expression)\" %S", args);
        if(hash_insert(hashval(car(args)), 
                        symval(CADR(args)), 
                        cons(l, CADR(args), CADR(cdr(args)))))
                                HALT(l, "%s", "out of memory");
        return car(args); 
}

static cell* subr_hcreate(lisp *l, cell *args) { 
        hashtable *ht;
        if(args->len % 2)
                RECOVER(l, "\"expected even number of arguments\" '%S", args);
        if(!(ht = hash_create(DEFAULT_LEN))) HALT(l, "%s", "out of memory");
        for(;!isnil(args); args = cdr(cdr(args))) {
                if(!isasciiz(car(args))) return mkerror();
                hash_insert(ht, symval(car(args)), cons(l, car(args), CADR(args)));
        }
        return mkhash(l, ht); 
}

static cell* subr_coerce(lisp *l, cell *args) { 
        char *fltend = NULL;
        intptr_t d = 0;
        size_t i = 0, j;
        cell *convfrom, *x, *y, *head;
        if(!cklen(args, 2) && issym(car(args))) goto fail;
        convfrom = CADR(args);
        if(intval(car(args)) == convfrom->type) return convfrom;
        switch(intval(car(args))) {
        case INTEGER: 
                    if(isstr(convfrom)) { /*int to string*/
                            if(!isnumber(strval(convfrom))) goto fail;
                            sscanf(strval(convfrom), "%"SCNiPTR, &d);
                    }
                    if(isfloat(convfrom)) /*float to string*/
                           d = (intptr_t) floatval(convfrom);
                    return mkint(l, d);
        case CONS:  if(isstr(convfrom)) { /*string to list of chars*/
                            head = x = cons(l, mknil(), mknil());
                            for(i = 0; i < convfrom->len; i++) {
                                char c[2] = {'\0', '\0'};
                                c[0] = strval(convfrom)[i];
                                y = mkstr(l, lstrdup(c));
                                setcdr(x, cons(l, y, mknil()));
                                x = cdr(x);
                            }
                            cdr(head)->len = i;
                            return cdr(head);
                    }
                    if(ishash(convfrom)) { /*hash to list*/
                            hashentry *cur;
                            hashtable *h = hashval(convfrom);
                            head = x = cons(l, mknil(), mknil());
                            for(j = 0, i = 0; i < h->len; i++)
                                    if(h->table[i])
                                            for(cur = h->table[i]; cur; cur = cur->next, j++) {
                                                    y = mkstr(l, lstrdup(cur->key));
                                                    setcdr(x, cons(l, y, mknil()));
                                                    x = cdr(x);
                                                    setcdr(x, cons(l, (cell*)cur->val, mknil()));
                                                    x = cdr(x);
                                            }
                            cdr(head)->len = j;
                            return cdr(head);
                    }
                    break;
        case STRING:if(isint(convfrom)) { /*int to string*/
                            char s[64] = "";
                            sprintf(s, "%"PRIiPTR, intval(convfrom));
                            return mkstr(l, lstrdup(s));
                    }
                    if(issym(convfrom)) /*symbol to string*/
                           return mkstr(l, lstrdup(strval(convfrom)));
                    if(isfloat(convfrom)) { /*float to string*/
                            char s[512] = "";
                            sprintf(s, "%f", floatval(convfrom));
                            return mkstr(l, lstrdup(s));
                    }
                    if(iscons(convfrom)) { /*list of chars/ints to string*/
                        /**@bug implement me*/
                    }
                    break;
        case SYMBOL:if(isstr(convfrom) && !strpbrk(strval(convfrom), " ;#()\t\n\r'\"\\"))
                            return intern(l, lstrdup(strval(convfrom)));
                    break;
        case HASH:  if(iscons(convfrom)) /*hash from list*/
                            return subr_hcreate(l, convfrom);
                    break;
        case FLOAT: if(isint(convfrom)) /*int to float*/
                          return mkfloat(l, intval(convfrom));
                    if(isstr(convfrom)) { /*string to float*/
                          lfloat d;
                          if(!isfnumber(strval(convfrom))) goto fail;
                          d = strtod(strval(convfrom), &fltend);
                          if(!fltend[0]) return mkfloat(l, d);
                          else           goto fail;
                    }
        default: break;
        }
fail:   RECOVER(l, "\"invalid conversion or argument length not 2\" %S", args);
        return mkerror();
}

static cell* subr_time(lisp *l, cell *args) {
        if(!cklen(args, 0))
                RECOVER(l, "\"expected ()\" %S", args);
        return mkint(l, time(NULL));
}

static cell* subr_date(lisp *l, cell *args) { /*not thread safe, also only GMT*/
        time_t raw;
        struct tm *gt;
        if(!cklen(args, 0))
                RECOVER(l, "\"expected ()\" %S", args);
        time(&raw);
        gt = gmtime(&raw);
        return cons(l,  mkint(l, gt->tm_year + 1900),
                cons(l, mkint(l, gt->tm_mon + 1),
                cons(l, mkint(l, gt->tm_mday),
                cons(l, mkint(l, gt->tm_hour),
                cons(l, mkint(l, gt->tm_min),
                cons(l, mkint(l, gt->tm_sec), mknil()))))));
}

static cell* subr_getenv(lisp *l, cell *args) {
        char *ret;
        if(!cklen(args, 1) || !isasciiz(car(args)))
                RECOVER(l, "\"expected (string)\" '%S", args);
        return (ret = getenv(strval(car(args)))) ? mkstr(l, lstrdup(ret)) : mknil();
}

static cell *subr_rand(lisp *l, cell *args) {
        if(!cklen(args, 0))
                RECOVER(l, "\"expected ()\" %S", args);
        return mkint(l, xorshift128plus(l->random_state));
}

static cell *subr_seed(lisp *l, cell *args) {
        if(!cklen(args, 2) || !isint(car(args)) || !isint(CADR(args)))
                RECOVER(l, "\"expected (integer integer)\" %S", args);
        l->random_state[0] = intval(car(args));
        l->random_state[1] = intval(CADR(args));
        return mktee();
}

static cell* subr_assoc(lisp *l, cell *args) {
        if(!cklen(args, 2) || !iscons(CADR(args)))
                RECOVER(l, "\"expected (val a-list)\" '%S", args);
        return assoc(car(args), CADR(args));
}

static cell *subr_setlocale(lisp *l, cell *args) {
        char *ret = NULL; /*this function is not reentrant, also should have more options*/
        if(!cklen(args, 2) || !isint(car(args)) || !isasciiz(CADR(args)))
                RECOVER(l, "\"expected (int string-or-symbol)\" '%S", args);
        switch(intval(car(args))) {
        case LC_ALL:      case LC_COLLATE: case LC_CTYPE:
        case LC_MONETARY: case LC_NUMERIC: case LC_TIME:
                ret = setlocale(intval(car(args)), strval(CADR(args)));
                break;
        default: RECOVER(l, "\"invalid int value\" '%S", args);
        }
        if(!ret) return mknil(); /*failed to set local*/
        return mkstr(l, lstrdup(ret));
}

static cell *subr_typeof(lisp *l, cell *args) {
        if(!cklen(args, 1))
                RECOVER(l, "\"expected (expr)\" %S", args);
        return mkint(l, car(args)->type);
}

static cell *subr_close(lisp *l, cell *args) {
        cell *x;
        if(!cklen(args, 1) || !isio(car(args)))
                RECOVER(l, "\"expected (io)\" %S", args);
        x = car(args);
        x->close = 1;
        io_close(ioval(x));
        return x;
}

static cell *subr_eval_time(lisp *l, cell *args) {
        clock_t start, end;
        lfloat used;
        cell *x;
        start = clock();
        x = subr_eval(l, args);
        end = clock();
        used = ((lfloat) (end - start)) / CLOCKS_PER_SEC;
        return cons(l, mkfloat(l, used), x);
}

static cell *subr_reverse(lisp *l, cell *args) {
        if(!cklen(args, 1)) goto fail;
        if(mknil() == car(args)) return mknil();
        switch(car(args)->type) {
        case STRING:
                {       
                        char *s = lstrdup(strval(car(args))), c;
                        size_t i = 0, len;
                        if(!s) HALT(l, "\"%s\"", "out of memory");
                        if(!(car(args)->len))
                                return mkstr(l, s);
                        len = car(args)->len - 1;
                        do {
                                c = s[i];
                                s[i] = s[len - i];
                                s[len - i] = c;
                        } while(i++ < (len / 2));
                        return mkstr(l, s);
                } break;
        case CONS:
                {
                        cell *x = car(args), *y = mknil();
                        if(!iscons(cdr(x)) && !isnil(cdr(x)))
                                return cons(l, cdr(x), car(x));
                        for(; !isnil(x); x = cdr(x)) 
                                y = cons(l, car(x), y);
                        return y;
                } break;
        case HASH: /**@bug implement me*/
                break;
        default:   
                break;
        }
fail:   RECOVER(l, "\"expected () (string) (list) (hash)\" %S", args);
        return mkerror();
}

static cell *subr_join(lisp *l, cell *args) {
        char *sep = "", *r, *tmp;
        if(args->len < 2 || !isasciiz(car(args))) 
                goto fail;
        sep = strval(car(args));
        if(!isasciiz(CADR(args))) {
                if(iscons(CADR(args))) {
                        args = CADR(args);
                        if(!isasciiz(car(args)))
                                goto fail;
                } else {
                        goto fail;
                }
        } else {
                args = cdr(args);
        }
        r = lstrdup(strval(car(args)));
        for(args = cdr(args); !isnil(args); args = cdr(args)) {
                if(!isasciiz(car(args))) {
                        free(r);
                        goto fail;
                }
                tmp = vstrcatsep(sep, r, strval(car(args)), NULL);
                free(r);
                r = tmp;
        }
        return mkstr(l, r);
fail:   RECOVER(l, "\"expected (string string...) or (string (string ...))\" %S", args);
        return mkerror();
}

static cell *subr_regexspan(lisp *l, cell *args) {
        regex_result rr;
        cell *m = mknil();
        if(!cklen(args, 2) || !isasciiz(car(args)) || !isasciiz(CADR(args)))
                RECOVER(l, "\"expected (string string)\" %S", args);
        rr = regex_match(strval(car(args)), strval(CADR(args)));
        if(rr.result <= 0)
                rr.start = rr.end = strval(CADR(args)) - 1;
        m = (rr.result < 0 ? mkerror() : (rr.result == 0 ? mknil() : mktee()));
        return cons(l, m, 
                cons(l, mkint(l, rr.start - strval(CADR(args))),
                cons(l, mkint(l, rr.end   - strval(CADR(args))), mknil())));
}

static cell *subr_raise(lisp *l, cell *args) {
        if(!cklen(args, 1) || !isint(car(args)))
                RECOVER(l, "\"expected (integer)\" %S", args);
        return raise(intval(car(args))) ? (cell*)mknil(): (cell*)mktee();
}

static cell *subr_split(lisp *l, cell *args) {
        char *pat, *s, *f;
        cell *op = mknil(), *head;
        regex_result rr;
        if(!cklen(args, 2) || !isasciiz(car(args)) || !isasciiz(CADR(args)))
                RECOVER(l, "\"expected (string string)\" %S", args);
        pat = strval(car(args));
        if(!(f = s = lstrdup(strval(CADR(args))))) 
                HALT(l, "\"%s\"", "out of memory");
        head = op = cons(l, mknil(), mknil());
        for(;;) {
                rr = regex_match(pat, s);
                if(!rr.result || rr.end == rr.start) {
                        setcdr(op, cons(l, mkstr(l, lstrdup(s)), mknil()));
                        break;
                }
                rr.start[0] = '\0';
                setcdr(op, cons(l, mkstr(l, lstrdup(s)), mknil()));
                op = cdr(op);
                s = rr.end;
        }
        free(f);
        return cdr(head);
}

static cell *subr_format(lisp *l, cell *args) {
        cell *cret;
        io *o = NULL, *t;
        char *fmt, c;
        int ret = 0;
        if(cklen(args, 0)) return mknil();
        if(isout(car(args))) {
                o = ioval(car(args));
                args = cdr(args);
        } else {
                o = l->ofp;
        }
        if(!isasciiz(car(args)))
                RECOVER(l, "\"expected () (io string expr...) (string expr...)\" %S", args);
        if(!(t = io_sout(calloc(2, 1), 2)))
                HALT(l, "\"%s\"", "out of memory");
        fmt = strval(car(args));
        args = cdr(args);
        while((c = *fmt++))
                if(ret == EOF) goto fail;
                else if('%' == c) {
                        switch((c = *fmt++)) {
                        case '\0': goto fail;
                        case '%':  ret = io_putc(c, t); 
                                   break;
                        case 's':  if(isnil(args) || !isasciiz(car(args)))
                                           goto fail;
                                   ret = io_puts(strval(car(args)), t);
                                   args = cdr(args);
                                   break;
                        case 'S':  if(isnil(args))
                                           goto fail;
                                   ret = printer(l, t, car(args), 0); 
                                   args = cdr(args);
                                   break;
                        default:   goto fail;
                        }
                } else {
                        ret = io_putc(c, t);
                }
        if(!isnil(args))
                goto fail;
        io_puts(t->p.str, o);
        cret = mkstr(l, t->p.str); /*t->p.str is not freed by io_close*/
        io_close(t);
        return cret;
fail:   free(t->p.str);
        io_close(t);
        RECOVER(l, "\"format error\" %S", args);
        return mkerror();
#undef  RESTORE_IO_STATE
}

