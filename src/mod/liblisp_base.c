/** @file       liblisp_base.c
 *  @brief      Miscellaneous functions
 *  @author     Richard Howe (2015)
 *  @license    LGPL v2.1 or Later
 *              <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html>
 *  @email      howe.r.j.89@gmail.com
 *
 *  @todo Add lock type and threading?
 **/
#include <lispmod.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include "utf8.h"

/**@brief Template for most of the functions in "math.h"
 * @param NAME name of math function such as "log", "sin", etc.*/
#define SUBR_MATH_UNARY(NAME, VALIDATION, DOCSTRING)\
static lisp_cell_t *subr_ ## NAME (lisp_t *l, lisp_cell_t *args) {\
        return mk_float(l, NAME (get_a2f(car(args))));\
}

#define MATH_UNARY_LIST\
	X(log,   "a", "natural logarithm")\
	X(fabs,  "a", "absolute value")\
	X(sin,   "a", "sine")\
	X(cos,   "a", "cosine")\
	X(tan,   "a", "tangent")\
	X(asin,  "a", "arcsine")\
	X(acos,  "a", "arcosine")\
	X(atan,  "a", "arctangent")\
	X(sinh,  "a", "hyperbolic sine")\
	X(cosh,  "a", "hyperbolic cosine")\
	X(tanh,  "a", "hyperbolic tangent")\
	X(exp,   "a", "exponential function")\
	X(sqrt,  "a", "square root")\
	X(ceil,  "a", "ceiling")\
	X(floor, "a", "floor")\
	X(log10, "a", "logarithm (base 10)")

#define X(FUNC, VALIDATION, DOCSTRING) SUBR_MATH_UNARY(FUNC, VALIDATION, DOCSTRING)
MATH_UNARY_LIST
#undef X
static lisp_cell_t *subr_pow(lisp_t * l, lisp_cell_t * args)
{
	return mk_float(l, pow(get_a2f(car(args)), get_a2f(CADR(args))));
}

static lisp_cell_t *subr_modf(lisp_t * l, lisp_cell_t * args)
{
	double x, fracpart, intpart = 0;
	x = get_a2f(car(args));
	fracpart = modf(x, &intpart);
	return cons(l, mk_float(l, intpart), mk_float(l, fracpart));
}

#define X(SUBR, VALIDATION, DOCSTRING) { # SUBR, VALIDATION, MK_DOCSTR( #SUBR, DOCSTRING), subr_ ## SUBR },
static lisp_module_subroutines_t math_primitives[] = {
	MATH_UNARY_LIST		/*all of the subr functions */
	{ "modf", "a", MK_DOCSTR("modf", "split a float into integer and fractional parts"), subr_modf},
	{ "pow", "a a", MK_DOCSTR("pow:", "raise a base to a power"), subr_pow},
	{ NULL, NULL, NULL, NULL}	/*must be terminated with NULLs */
};
#undef X

#define INTEGER_XLIST\
	X("*lc-all*",       LC_ALL)       X("*lc-collate*",  LC_COLLATE)\
	X("*lc-ctype*",     LC_CTYPE)     X("*lc-monetary*", LC_MONETARY)\
	X("*lc-numeric*",   LC_NUMERIC)   X("*lc-time*",     LC_TIME)\
	X("*float-radix*",  FLT_RADIX)    X("*float-rounds*", FLT_ROUNDS)\
	X("*integer-max*",  INTPTR_MAX)   X("*integer-min*",  INTPTR_MIN)\
	X("*random-max*",   INTPTR_MAX)\
	X("*integer-bits*", (CHAR_BIT * sizeof(intptr_t)))\
	X("*trace-off*",    LISP_LOG_LEVEL_OFF)\
	X("*trace-errors*", LISP_LOG_LEVEL_ERROR)\
	X("*trace-notes*",  LISP_LOG_LEVEL_NOTE)\
	X("*trace-debug*",  LISP_LOG_LEVEL_DEBUG)

#define FLOAT_XLIST\
	X("pi", 3.14159265358979323846) X("e", 2.71828182845904523536)\
	X("*epsilon*",   LFLT_EPSILON)  X("*float-smallest*", LFLT_MIN)\
	X("*float-biggest*", LFLT_MAX)

#define X(NAME, VAL) { NAME, VAL },
static const struct float_list { char *name; lisp_float_t val; } floats[] = {
/**@brief A list of all floating point values to be made available to the
 *        interpreter as lisp objects */
        FLOAT_XLIST
        {NULL, 0.0 }
};
#undef X

#define X(NAME, VAL) { NAME, VAL },
/**@brief A list of all integer values to be made available to the
 *        interpreter as lisp objects */
static const struct integer_list { char *name; intptr_t val; } integers[] = {
        INTEGER_XLIST
        {NULL, 0}
};
#undef X

#define SUBR_ISX(NAME)\
static lisp_cell_t *subr_ ## NAME (lisp_t *l, lisp_cell_t *args) { UNUSED(l);\
        char *s, c;\
        if(is_int(car(args)))\
                return NAME (get_int(car(args))) ? gsym_tee() : gsym_nil();\
        s = get_str(car(args));\
        if(!s[0]) return gsym_nil();\
        while((c = *s++)) \
                if(! NAME (c))\
                        return gsym_nil();\
        return gsym_tee();\
}

/**@todo give these functions better names*/
#define ISX_LIST\
        X("is-alphanumeric", isalnum,  "Is a string or integer composed of alphanumeric characters?")\
        X("is-alpha",        isalpha,  "Is a string or integer composed of alphabetic characters?")\
        X("is-control-character", iscntrl,  "Is a string or integer composed of control characters?")\
        X("is-digit",       isdigit,  "Is a string or integer composed of digits?")\
        X("is-printable-excluding-space", isgraph,  "Is a string or integer composed of printable characters (excluding space)?")\
        X("is-lowercase",   islower,  "Is a string or integer composed of lower case characters?")\
        X("is-printable",   isprint,  "Is a string or integer composed of printable characters?")\
        X("is-punctuation", ispunct,  "Is a string or integer composed of punctuation characters?")\
        X("is-space",       isspace,  "Is a string or integer composed of whitespace characters?")\
        X("is-uppercase",   isupper,  "Is a string or integer composed of upper case characters?")\
        X("is-hex-digit",   isxdigit, "Is a string or integer composed of hexadecimal digits?")

#define X(NAME, FUNC, IGNORE) SUBR_ISX(FUNC)
ISX_LIST /*defines lisp subroutines for checking whether a string only contains a character class*/
#undef X

#define SUBROUTINE_XLIST\
	X("crc",        subr_crc,        "Z",   "CRC-32 of a string")\
	X("hash",       subr_hash,       "Z",   "hash a string")\
	X("date",       subr_date,       "",    "return a list representing the date (GMT) (not thread safe)")\
	X("documentation",  subr_doc_string, "x",   "return the documentation string from a procedure")\
	X("errno",      subr_errno,      "",    "return the current errno")\
	X("gc",         subr_gc,         "",    "force the collection of garbage")\
	X("ilog2",      subr_ilog2,      "d",   "compute the binary logarithm of an integer")\
	X("ipow",       subr_ipow,       "d d", "compute the integer exponentiation of two numbers")\
	X("set-locale", subr_setlocale,  "d Z", "set the locale, this affects global state!")\
	X("procedure-arguments",  subr_proc_args,  "l",   "return the arguments for a lambda or F-expression")\
	X("procedure-code",  subr_proc_code,  "l",   "return the code from a lambda or F-expression")\
	X("procedure-environment",   subr_proc_env,   "l",   "return the environment captured for a lambda or F-expression")\
	X("random",     subr_rand,       "",    "return a pseudo random number generator")\
	X("seed",       subr_seed,       "d d", "seed the pseudo random number generator")\
	X("string-not-span", subr_strcspn,    "Z Z", "offset into first string of first occurrence of character in second string")\
	X("errno->string",   subr_strerror,   "d",   "convert an errno into a string describing that error")\
	X("string-span",     subr_strspn,     "Z Z", "offset into first string of last occurrence of character in second string")\
	X("strstr",     subr_strstr,     "Z Z", "return offset of first occurrence of second string in the first")\
	X("system",     subr_system,     NULL,  "execute a command with the system command interpreter")\
	X("timed-eval", subr_timed_eval, "A",   "time an evaluation")\
	X("time",       subr_time,       "",    "create a list representing the time")\
	X("validation-string", subr_validation_string, "x", "return the format string from a procedure")\
	X("validate",   subr_validate,  "d Z c", "validate an argument list against a format string")\
	X("is-utf8",    subr_isutf8,     "Z",   "return t if string is valid utf8, nil otherwise")\
	X("utf8-length", subr_utf8_length, "Z", "return length of UTF-8 string, or error if UTF-8 validation fails")\
	X("utf8-strchr", subr_utf8_strchr, "C Z", "return index into first occurrence of character in UTF-8 string")

#define X(NAME, SUBR, VALIDATION , DOCSTRING) static lisp_cell_t * SUBR (lisp_t *l, lisp_cell_t *args);
SUBROUTINE_XLIST		/*function prototypes for all of the built-in subroutines */
#undef X

#define X(NAME, SUBR, VALIDATION, DOCSTRING) { NAME, VALIDATION, MK_DOCSTR(NAME, DOCSTRING), SUBR },
static lisp_module_subroutines_t main_primitives[] = {
	SUBROUTINE_XLIST		/*all of the subr functions */
	{ NULL, NULL, NULL, NULL}	/*must be terminated with NULLs */
};
#undef X

/**** Module C Helper / Functionality functions *******************************/

/**@todo These need locking!*/
static uint32_t crc_table[256];	/* Table of CRCs of all 8-bit messages. */
static int crc_table_computed = 0;	/* Flag: has the table been computed? */
static uint64_t xorshift128plus_state[2] /**< PRNG state */;

static int32_t ilog2(uint64_t v)
{
	if (!v)
		return INT32_MIN;
	int32_t r = 0;
	while (v >>= 1)
		r++;
	return r;
}

static uint64_t ipow(uint64_t base, uint64_t exp)
{
	uint64_t result = 1;
	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

static uint64_t xorshift128plus(uint64_t s[2])
{
	/*PRNG*/ uint64_t x = s[0];
	uint64_t const y = s[1];
	s[0] = y;
	x ^= x << 23;		/*a */
	x ^= x >> 17;		/*b */
	x ^= y ^ (y >> 26);	/*c */
	s[1] = x;
	return x + y;
}

static void make_crc_table(void)
{ /* Make the table for a fast CRC. */
	uint32_t c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (uint32_t) n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
		crc_table[n] = c;
	}
	crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
should be initialized to all 1's, and the transmitted value
is the 1's complement of the final running CRC (see the
crc() routine below)). */
static uint32_t crc_update(uint32_t crc, uint8_t * abuf, size_t len)
{
	uint32_t c = crc;
	size_t n;

	if (!crc_table_computed)
		make_crc_table();
	for (n = 0; n < len; n++) {
		c = crc_table[(c ^ abuf[n]) & 0xff] ^ (c >> 8);
	}
	return c;
}

/*static uint32_t crc(uint8_t * abuf, size_t len)
{ // Return the CRC of the bytes abuf[0..len-1].
	return crc_update(0xffffffffL, abuf, len) ^ 0xffffffffL;
}*/

static uint32_t crc_init(uint8_t * abuf, size_t len)
{
	return crc_update(0xffffffffL, abuf, len);
}

static uint32_t crc_final(uint32_t crc)
{
	return crc ^ 0xffffffffL;
}

/******************************************************************************/

static lisp_cell_t *subr_utf8_strchr(lisp_t * l, lisp_cell_t * args)
{
	char *utf8str = get_str(CADR(args));
	char *res;
	size_t utf8len  = get_length(CADR(args));
	int i = 0;
	uint32_t utf8char;

	if(utf8_validate(utf8str, utf8len) == UTF8_REJECT)
		return gsym_error();
	if(is_asciiz(car(args))) {
		if(utf8_validate(get_str(car(args)), get_length(car(args))) == UTF8_REJECT)
			return gsym_error();
		utf8char = utf8_nextchar(get_str(car(args)), &i);
	} else { /**@todo perform validation on integer*/
		utf8char = get_int(car(args));
	}
	i = 0;
	if((res = utf8_strchr(utf8str, utf8char, &i)))
		return mk_int(l, res - utf8str);
	return gsym_nil();
}

static lisp_cell_t *subr_utf8_length(lisp_t * l, lisp_cell_t * args)
{
	if(utf8_validate(get_str(car(args)), get_length(car(args))) == UTF8_REJECT)
		return gsym_error();
	return mk_int(l, utf8_strlen(get_str(car(args))));
}

static lisp_cell_t *subr_isutf8(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	if(utf8_validate(get_str(car(args)), get_length(car(args))) == UTF8_ACCEPT)
		return gsym_tee();
	return gsym_nil();
}

static lisp_cell_t *subr_validate(lisp_t * l, lisp_cell_t * args)
{
	return LISP_VALIDATE_ARGS(l, "validate", get_int(car(args)), get_str(CADR(args)), CADDR(args), 0) ?
			gsym_tee() : gsym_nil();
}

static lisp_cell_t *subr_proc_code(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	return car(get_proc_code(car(args)));
}

static lisp_cell_t *subr_proc_args(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	return get_proc_args(car(args));
}

static lisp_cell_t *subr_proc_env(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	return get_proc_env(car(args));
}

static lisp_cell_t *subr_validation_string(lisp_t * l, lisp_cell_t * args)
{
	char *s = get_func_format(car(args));
	return s ? mk_str(l, lisp_strdup(l, s)) : gsym_nil();
}

static lisp_cell_t *subr_doc_string(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	return get_func_docstring(car(args));
}

static lisp_cell_t *subr_system(lisp_t * l, lisp_cell_t * args)
{
	if (lisp_check_length(args, 0))
		return mk_int(l, system(NULL));
	if (lisp_check_length(args, 1) && is_asciiz(car(args)))
		return mk_int(l, system(get_str(car(args))));
	LISP_RECOVER(l, "\"expected () or (string)\"\n '%S", args);
	return gsym_error();
}

static lisp_cell_t *subr_gc(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(args);
	lisp_gc_mark_and_sweep(l);
	return gsym_tee();
}

static lisp_cell_t *subr_ilog2(lisp_t * l, lisp_cell_t * args)
{
	return mk_int(l, ilog2(get_int(car(args))));
}

static lisp_cell_t *subr_ipow(lisp_t * l, lisp_cell_t * args)
{
	return mk_int(l, ipow(get_int(car(args)), get_int(CADR(args))));
}

static lisp_cell_t *subr_rand(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(args);
	return mk_int(l, xorshift128plus(xorshift128plus_state));
}

static lisp_cell_t *subr_seed(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	xorshift128plus_state[0] = get_int(car(args));
	xorshift128plus_state[1] = get_int(CADR(args));
	return gsym_tee();
}

static lisp_cell_t *subr_timed_eval(lisp_t * l, lisp_cell_t * args)
{
	clock_t start, end;
	lisp_float_t used;
	lisp_cell_t *x;
	start = clock();
	x = lisp_eval(l, car(args)); /**@todo pass in environment*/
	end = clock();
	used = ((lisp_float_t) (end - start)) / CLOCKS_PER_SEC;
	return cons(l, mk_float(l, used), x);
}

static lisp_cell_t *subr_errno(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(args);
	int e = errno;
	errno = 0;
	return mk_int(l, e);
}

static lisp_cell_t *subr_strerror(lisp_t * l, lisp_cell_t * args)
{
	return mk_str(l, lisp_strdup(l, strerror(get_int(car(args)))));
}

static lisp_cell_t *subr_strstr(lisp_t *l, lisp_cell_t *args)
{
	char *s = get_str(car(args));
	char *p = get_str(CADR(args));
	char *r = strstr(s, p);
	if(!r)
		return gsym_nil();
	return mk_int(l, r-s);
}

static lisp_cell_t *subr_strcspn(lisp_t *l, lisp_cell_t *args)
{
	return mk_int(l, strcspn(get_str(car(args)), get_str(CADR(args))));
}

static lisp_cell_t *subr_strspn(lisp_t *l, lisp_cell_t *args)
{
	return mk_int(l, strspn(get_str(car(args)), get_str(CADR(args))));
}

static lisp_cell_t *subr_time(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(args);
	return mk_int(l, time(NULL));
}

static lisp_cell_t *subr_date(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(args);
	time_t raw;
	struct tm *gt;
	/**@todo locking */
	time(&raw);
	gt = gmtime(&raw);
	return mk_list(l, mk_int(l, gt->tm_year + 1900), mk_int(l, gt->tm_mon),
		       mk_int(l, gt->tm_wday), mk_int(l, gt->tm_mday),
		       mk_int(l, gt->tm_hour), mk_int(l, gt->tm_min), mk_int(l, gt->tm_sec), NULL);
}

static lisp_cell_t *subr_setlocale(lisp_t * l, lisp_cell_t * args)
{
	char *ret = NULL;
	switch (get_int(car(args))) {
	case LC_ALL:
	case LC_COLLATE:
	case LC_CTYPE:
	case LC_MONETARY:
	case LC_NUMERIC:
	case LC_TIME:
		ret = setlocale(get_int(car(args)), get_str(CADR(args)));
		break;
	default:
		LISP_RECOVER(l, "\"invalid int value\"\n '%S", args);
	}
	if (!ret)
		return gsym_nil();	/*failed to set local */
	return mk_str(l, lisp_strdup(l, ret));
}

static lisp_cell_t *subr_crc(lisp_t * l, lisp_cell_t * args)
{
	UNUSED(l);
	uint32_t c;
	c = crc_final(crc_init((uint8_t *) get_str(car(args)), get_length(car(args))));
	return mk_int(l, c);
}

static lisp_cell_t *subr_hash(lisp_t * l, lisp_cell_t * args)
{
	return mk_int(l, djb2(get_str(car(args)), get_length(car(args))));
}

int lisp_module_initialize(lisp_t *l)
{
	size_t i = 0;
	assert(l);

	make_crc_table();
        xorshift128plus_state[0] = 0xCAFEBABE; /*Are these good seeds?*/
        xorshift128plus_state[1] = 0xDEADC0DE;
        for(size_t i = 0; i < 4096; i++) /*discard first N numbers*/
                (void)xorshift128plus(xorshift128plus_state);

#define X(NAME, FUNC, DOCSTRING) if(!lisp_add_subr(l, NAME, subr_ ## FUNC, "C", MK_DOCSTR(# FUNC "?", DOCSTRING))) goto fail;
ISX_LIST /*add all of the subroutines for string character class testing*/
#undef X

        for(i = 0; integers[i].name; i++) /*add all integers*/
                if(!lisp_add_cell(l, integers[i].name, mk_int(l, integers[i].val)))
                        goto fail;

        for(i = 0; floats[i].name; i++) /*add all floats*/
                if(!lisp_add_cell(l, floats[i].name, mk_float(l, floats[i].val)))
                        goto fail;

	if(lisp_add_module_subroutines(l, math_primitives, 0) < 0)
		return -1;

	return lisp_add_module_subroutines(l, main_primitives, 0);
fail:
	return -1;
}

#ifdef __unix__
static void construct(void) __attribute__ ((constructor));
static void destruct(void) __attribute__ ((destructor));
static void construct(void) {}
static void destruct(void)  {}
#elif _WIN32
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	UNUSED(hinstDLL);
	UNUSED(lpvReserved);
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	default:
		break;
	}
	return TRUE;
}
#endif
