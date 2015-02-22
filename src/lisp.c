/**
 *  @file           lisp.c
 *  @brief          The Lisp Interpreter
 *  @author         Richard James Howe.
 *  @copyright      Copyright 2013 Richard James Howe.
 *  @license        LGPL v2.1 or later version
 *  @email          howe.r.j.89@gmail.com
 *  @details
 *
 *  Experimental, small, lisp interpreter.
 *
 *  At a minimum I should define; quote, atom, eq, car, cdr, cons, 
 *  cond, define (or label) and lambda.
 *
 **/

#include "lisp.h"
#include <string.h>
#include <assert.h>
#include "mem.h"
#include "gc.h"
#include "sexpr.h"
#include "regex.h"
#include "hash.h"

#define UNUSED(X)  (void)(X)
#define HASH_SIZE  (128u)

typedef struct{
        const char *s;
        primitive_f func;
} primop_initializers;

static size_t list_len(expr x);
static expr mknil(void);
static expr find(expr env, expr x, lisp l);
static expr extend(expr sym, expr val, lisp l);
static expr extendprimop(const char *s, primitive_f func, lisp l);
static expr mkobj(sexpr_e type);
static expr mksym(char *s);
static expr mkprimop(primitive_f func);
static expr mkproc(expr args, expr code, expr env);

/** 
 * @brief List of primitive operations, used for initialization of structures 
 *        and function declarations. It uses X-Macros to achieve this job.
 *        See  <https://en.wikipedia.org/wiki/X_Macro> and
 *        <http://www.drdobbs.com/cpp/the-x-macro/228700289>
 **/
#define LIST_OF_PRIMITIVE_OPERATIONS\
        PRIMOP_X("+",        primop_add)\
        PRIMOP_X("-",        primop_sub)\
        PRIMOP_X("*",        primop_prod)\
        PRIMOP_X("/",        primop_div)\
        PRIMOP_X("atom",     primop_atom)\
        PRIMOP_X("mod",      primop_mod)\
        PRIMOP_X("eq",       primop_eq)\
        PRIMOP_X("car",      primop_car)\
        PRIMOP_X("cdr",      primop_cdr)\
        PRIMOP_X("cons",     primop_cons)\
        PRIMOP_X("nth",      primop_nth)\
        PRIMOP_X("length",   primop_len)\
        PRIMOP_X("=",        primop_numeq)\
        PRIMOP_X("print",    primop_printexpr)\
        PRIMOP_X("eqt",      primop_typeeq)\
        PRIMOP_X("reverse",  primop_reverse)\
        PRIMOP_X("system",   primop_system)\
        PRIMOP_X("match",    primop_match)
 
/** @brief built in primitives, static declarations **/
#define PRIMOP_X(STRING, FUNCTION) static expr FUNCTION(expr args);
LIST_OF_PRIMITIVE_OPERATIONS
#undef PRIMOP_X

/** @brief initializer table for primitive operations **/
#define PRIMOP_X(STRING, FUNCTION) {STRING, FUNCTION},
static primop_initializers primops[] = {
        LIST_OF_PRIMITIVE_OPERATIONS
        {NULL,       NULL} /* this *has* to be the last entry */
};
#undef PRIMOP_X

#define CAR(X)                  ((X)->data.cons[0])
#define CDR(X)                  ((X)->data.cons[1])
#define SETCAR(X,Y)             ((X)->data.cons[0] = (Y))
#define SETCDR(X,Y)             ((X)->data.cons[1] = (Y))
#define CMPSYM(EXPR,STR)        (!strcmp(CAR((EXPR))->data.symbol,(STR)))
#define ISNIL(X)                (S_NIL == (X)->type)                    

/*** interface functions *****************************************************/

/**
 *  @brief          Initialize the lisp interpreter
 *  @return         A fully initialized lisp environment
 **/
lisp lisp_init(void)
{
        lisp l;
        size_t i;
        if(!(l = mem_calloc(sizeof(*l)))) goto fail;
        if(!(l->global = hash_create(HASH_SIZE))) goto fail;
        if(!(l->env = mem_calloc(sizeof(sexpr_t)))) goto fail;

        if(!(l->i = mem_calloc(io_sizeof_io()))) goto fail;
        if(!(l->o = mem_calloc(io_sizeof_io()))) goto fail;
        l->env->type = S_CONS;

        /* set up file initial I/O */
        io_file_in(l->i, stdin);
        io_file_out(l->o, stdout);
        io_file_out(io_get_error_stream(), stderr); 

        /* normal forms, kind of  */
        for(i = 0; (NULL != primops[i].s) && (NULL != primops[i].func) ; i++)
                if(NULL == extendprimop(primops[i].s, primops[i].func, l))
                        goto fail;
        return l;
fail:
        IO_REPORT("initilization failed");
        return NULL; 
}

/** 
 *  @brief      Registers a function for use within the lisp environment    
 *  @param      name    functions name
 *  @param      func    function to register.
 *  @param      l       lisp environment to register function in
 *  @return     int     Error code, 0 = Ok, >0 is a failure.
 */
int lisp_register_function(char *name, expr(*func) (expr args, lisp l), lisp l){
        UNUSED(name); UNUSED(func); UNUSED(l);
        return 1;
}

/** 
 *  @brief    lisp_repl implements a lisp Read-Evaluate-Print-Loop
 *  @param    l an initialized lisp environment
 *  @return   Always zero at the moment
 *
 *  @todo When Error Expression have been properly implemented any
 *        errors that have not been caught should be returned by lisp_repl
 *        or handled by it to avoid multiple error messages being printed
 *        out.
 */
lisp lisp_repl(lisp l)
{
        expr x;
        while (NULL != (x = sexpr_parse(l->i))) {
                x = lisp_eval(x, l->env, l);
                sexpr_print(x, l->o, 0);
                lisp_clean(l);
        }
        return l;
}

/**
 *  @brief          Destroy and clean up a lisp environment
 *  @param          l   initialized lisp environment
 *  @return         void
 **/
void lisp_end(lisp l)
{
        fflush(NULL);
        gc_sweep(); /*do not call mark before **this** sweep */ 
        io_file_close(l->o);
        io_file_close(l->i);
        free(l->o);
        free(l->i);
        free(l->env);
        hash_destroy(l->global);
        mem_free(l);
        return;
}

/**
 *  @brief          Read in an s-expression 
 *  @param          i   Read input from...
 *  @return         A valid s-expression (or NULL), which might be an error!
 **/
expr lisp_read(io * i) { return sexpr_parse(i); }

/**
 *  @brief          Print out an s-expression
 *  @param          x   Expression to print
 *  @param          o   Output stream
 *  @return         void
 **/
void lisp_print(expr x, io * o) { sexpr_print(x, o, 0); }

/**
 *  @brief          Evaluate an already parsed lisp expression
 *  @param          x   The s-expression to parse
 *  @param          env The environment to lisp_evaluate in
 *  @param          l   The global lisp environment
 *  @return         An lisp_evaluated expression, possibly ready for printing.
 **/
expr lisp_eval(expr x, expr env, lisp l)
{
        expr nx;
        if(NULL == x){
                SEXPR_PERROR(NULL,"lisp_eval passed NULL");
                exit(EXIT_FAILURE);
        }

START_EVAL:
        switch(x->type){
        case S_NIL:    case S_TEE:       case S_INTEGER: 
        case S_STRING: case S_PRIMITIVE: case S_PROC: 
        case S_HASH:   case S_LISP_ENV: 
                return x; 
        case S_QUOTE:
                return x->data.quoted;
        case S_CONS: 
        if(S_SYMBOL == CAR(x)->type){
                if (CMPSYM(x,"begin")){
                        expr y = NULL;
                        if(1 == list_len(x))
                                return mknil();
                        for(;;){
                                x = CDR(x);
                                if(NULL == CDR(x)){
                                /*if(ISNIL(CDR(x))){*/
                                        x = y;
                                        goto START_EVAL;
                                }
                                y = lisp_eval(CAR(x),env,l);
                        }
                } else if (CMPSYM(x,"define")){
                        expr nx;
                        if(list_len(x) != 3){
                                SEXPR_PERROR(x,"define: argc != 3");
                                return mknil();
                        }
                        (void)extend(CAR(CDR(x)), nx = lisp_eval(CAR(CDR(CDR(x))),env, l), l);
                        return nx;
                } else if (CMPSYM(x,"if")){
                        if(list_len(x) != 4){
                                SEXPR_PERROR(x,"if: argc != 4");
                                return mknil();
                        }
                        nx = lisp_eval(CAR(CDR(x)),env,l);
                        if(!ISNIL(nx)){
                                x = CAR(CDR(CDR(x)));
                        } else {
                                x = CAR(CDR(CDR(CDR(x))));
                        }
                        goto START_EVAL;
                } else if (CMPSYM(x,"lambda")){
                        if(list_len(x) != 3){
                                SEXPR_PERROR(x,"lambda: argc != 3");
                                return mknil();
                        }
                        if(S_CONS != CAR(CDR(x))->type){
                                SEXPR_PERROR(x,"lambda; expected argument list");
                                return mknil();
                        }
                        return mkproc(CAR(CDR(x)),CDR(CDR(x)), env);
                } else if (CMPSYM(x,"quote")){
                        if(list_len(x) != 2){
                                SEXPR_PERROR(x,"quote: argc != 2");
                                return mknil();
                        }
                        return CAR(CDR(x));
                } else if (CMPSYM(x,"set")){
                } else {
                        expr procedure = lisp_eval(CAR(x), env, l);
                        if(!ISNIL(procedure))
                                return procedure;
                        else
                                return mknil();
                }
        }
        SEXPR_PERROR(x,"cannot apply");
        return mknil();
        case S_SYMBOL:
                nx = find(env, x, l);
                if(NULL == nx){
                        SEXPR_PERROR(x, "unbound symbol");
                        return mknil();
                }
                return nx;
        case S_FILE:      /*fall through, not a type*/
        case S_ERROR:     IO_REPORT("Not implemented");
        case S_LAST_TYPE: /*fall through, not a type*/
        default:
                IO_REPORT("Not a valid type");
                exit(EXIT_FAILURE);
        }

        SEXPR_PERROR(NULL,"Should never get here.");
        return x;
}

/**
 *  @brief          Garbage collection
 *  @param          l   Lisp environment to mark and collect
 *  @return         void
 **/
void lisp_clean(lisp l)
{
        /*gc_mark(l->global_head);*/
        gc_sweep();
}

/*** internal helper functions ***********************************************/

/** calculate length of a list **/
static size_t list_len(expr x){
        size_t i;
        for(i = 0; (NULL != x) && (S_NIL != x->type); x = CDR(x))
                i++;
        return i-1;
}

/** find a symbol in an environment **/
static expr find(expr env, expr x, lisp l){
        return hash_lookup(l->global, x->data.symbol);
}

/** extend the lisp environment **/
static expr extend(expr sym, expr val, lisp l){
        /*XXX: check*/
        hash_insert(l->global, sym->data.symbol, val);
        return val;
}

/** extend the lisp environment with a primitive operator **/
static expr extendprimop(const char *s, expr(*func) (expr args), lisp l)
{
        return extend(mksym(mem_strdup(s)), mkprimop(func), l);
}

/** make new object **/
static expr mkobj(sexpr_e type)
{
        expr nx;
        nx = gc_calloc();
        nx->len = 0;
        nx->type = type;
        return nx;
}

/** make a nil **/
static expr mknil(void){
        return mkobj(S_NIL);
}

/** make a new symbol **/
static expr mksym(char *s)
{
        expr nx;
        nx = mkobj(S_SYMBOL);
        nx->len = strlen(s);
        nx->data.symbol = s;
        return nx;
}

/** make a new primitive **/
static expr mkprimop(expr(*func) (expr args))
{
        expr nx;
        nx = mkobj(S_PRIMITIVE);
        nx->data.func = func;
        return nx;
}

/** make a new procedure **/
static expr mkproc(expr args, expr code, expr env){
        return mknil();
}

/*** primitive operations ****************************************************/

/**macro helpers for primops**/
#define INTCHK_R(EXP)\
  if(S_INTEGER!=((EXP)->type)){\
    SEXPR_PERROR((EXP),"arg != integer");\
    return nil;\
  }

/**true if arg is an atom, nil otherwise**/
static expr primop_atom(expr args)
{UNUSED(args); return mknil();}

/**add a list of numbers**/
static expr primop_add(expr args)
{UNUSED(args); return mknil();}

/**subtract a list of numbers from the 1 st arg**/
static expr primop_sub(expr args)
{UNUSED(args); return mknil();}

/**multiply a list of numbers together**/
static expr primop_prod(expr args)
{UNUSED(args); return mknil();}

/**divide the first argument by a list of numbers**/
static expr primop_div(expr args)
{UNUSED(args); return mknil();}

/**arg_1 modulo arg_2**/
static expr primop_mod(expr args)
{UNUSED(args); return mknil();}

/**car**/
static expr primop_car(expr args)
{UNUSED(args); return mknil();}

/**cdr**/
static expr primop_cdr(expr args)
{UNUSED(args); return mknil();}

/**cons**/
static expr primop_cons(expr args)
{UNUSED(args); return mknil();}

/**NTH element in a list or string**/
static expr primop_nth(expr args)
{UNUSED(args); return mknil();}

/**length of a list or string**/
static expr primop_len(expr args)
{UNUSED(args); return mknil();}

/**test equality of the 1st arg against a list of numbers**/
static expr primop_numeq(expr args)
{UNUSED(args); return mknil();}

/**print**/
static expr primop_printexpr(expr args)
{UNUSED(args); return mknil();}

/**strict equality**/
static expr primop_eq(expr args)
{UNUSED(args); return mknil();}

/**type equality**/
static expr primop_typeeq(expr args)
{UNUSED(args); return mknil();}

/**reverse a list or a string**/
static expr primop_reverse(expr args)
{UNUSED(args); return mknil();}

static expr primop_system(expr args)
{UNUSED(args); return mknil();}

static expr primop_match(expr args)
{UNUSED(args); return mknil();}

#undef INTCHK_R
#undef UNUSED

/*****************************************************************************/
