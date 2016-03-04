/** @file       valid.c
 *  @brief      validate a list against a type format string, see the
 *              "liblisp.h" header or the code for the format options.
 *  @author     Richard Howe (2015)
 *  @license    LGPL v2.1 or Later
 *  @email      howe.r.j.89@gmail.com 
 *  
 *  @todo    Add checks for *specific* user defined values, this will
 *           require turning lisp_validate_args into a variadic function, 
 *           something like %u will pop an integer off the argument stack 
 *           to test against.
 *  @todo    Grouped format specifiers should be treated as an "or", or they
 *           should be be treated as an "and", with the length being calculated
 *           correctly, which it is not.
 *  @warning The number of arguments in the string and the number of arguments
 *           passed into the validation string must be the same, this is the
 *           responsibility of the user of these functions
 **/
 
#include "liblisp.h"
#include "private.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define VALIDATE_XLIST\
        X('s', "symbol",            is_sym(x))\
        X('d', "integer",           is_int(x))\
        X('c', "cons",              is_cons(x))\
        X('L', "cons-or-nil",       is_cons(x) || is_nil(x))\
        X('p', "procedure",         is_proc(x))\
        X('r', "subroutine",        is_subr(x))\
        X('S', "string",            is_str(x))\
        X('P', "io-port",           is_io(x))\
        X('h', "hash",              is_hash(x))\
        X('F', "f-expr",            is_fproc(x))\
        X('f', "float",             is_floating(x))\
        X('u', "user-defined",      is_userdef(x))\
        X('b', "t-or-nil",          is_nil(x) || x == gsym_tee())\
        X('i', "input-port",        is_in(x))\
        X('o', "output-port",       is_out(x))\
        X('Z', "symbol-or-string",  is_asciiz(x))\
        X('a', "integer-or-float",  is_arith(x))\
        X('x', "function",          is_func(x))\
        X('I', "input-port-or-string", is_in(x) || is_str(x))\
        X('l', "defined-procedure", is_proc(x) || is_fproc(x))\
        X('C', "symbol-string-or-integer", is_asciiz(x) || is_int(x))\
        X('A', "any-expression",    1)

static int print_type_string(lisp *l, const char *msg, unsigned len, const char *fmt, cell *args)
{
        const char *s, *head = fmt;
        char c;
        io *e = lisp_get_logging(l);
        msg = msg ? msg : "";
        lisp_printf(l, e, 0, 
                "\n(%Berror%t\n %y'validation\n %r\"%s\"\n%t '(%yexpected-length %r%d%t)\n '(%yexpected-arguments%t ", 
                msg, (intptr_t)len); 
        while((c = *fmt++)) {
                s = "";
                switch(c) {
                case ' ': continue;
#define X(CHAR, STRING, ACTION) case (CHAR): s = (STRING); break;
                VALIDATE_XLIST
#undef X
                default: RECOVER(l, "\"invalid format string\" \"%s\" %S))", head, args);
                }
                lisp_printf(l, e, 0, "%y'%s%t", s);
                if(*fmt) io_putc(' ', e);
        }
        return lisp_printf(l, e, 1, ") %S)\n", args);
}

size_t validate_arg_count(const char *fmt)
{
	size_t i = 0;
	if (!fmt)
		return 0;
	for (; *fmt; i++) {
		while (*fmt && isspace(*fmt++)) ;
		while (*fmt && !isspace(*fmt++)) ;
	}
	return i;
}

int lisp_validate_cell(lisp * l, cell * x, cell * args, int recover)
{
	char *fmt, *msg;
	cell *ds;
	assert(x && is_func(x));
	ds = get_func_docstring(x);
	msg = get_str(ds);
	msg = msg ? msg : "";
	fmt = get_func_format(x);
	if (!fmt)
		return 1;	/*as there is no validation string, its up to the function */
	return lisp_validate_args(l, msg, get_length(x), fmt, args, recover);
}

int lisp_validate_args(lisp * l, const char *msg, unsigned len, const char *fmt, cell * args, int recover)
{
	assert(l && fmt && args && msg);
	int v = 1;
	char c;
	const char *fmt_head;
	cell *args_head, *x;
	assert(l && fmt && args);
	args_head = args;
	fmt_head = fmt;
	if (!cklen(args, len))
		goto fail;
	while ((c = *fmt++)) {
		if (is_nil(args) || !v || is_closed(car(args)))
			goto fail;
		v = 0;
		x = car(args);
		switch (c) {
		case ' ':
			v = 1;
			continue;
#define X(CHAR, STRING, ACTION) case (CHAR): v = ACTION; break;
			VALIDATE_XLIST
#undef X
		default:
			RECOVER(l, "\"%s\"", "invalid validation format");
		}
		args = cdr(args);
	}
	if (!v)
		goto fail;
	return 1;
 fail:	
        print_type_string(l, msg, len, fmt_head, args_head);
	if (recover)
		lisp_throw(l, 1);
	return 0;
}

