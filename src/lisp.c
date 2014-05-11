/**
 *  @file           lisp.c
 *  @brief          The Lisp Interpreter; Lispy Space Princess
 *  @author         Richard James Howe.
 *  @copyright      Copyright 2013 Richard James Howe.
 *  @license        GPL v3.0
 *  @email          howe.r.j.89@gmail.com
 *  @details
 *
 *  Experimental, small, lisp interpreter.
 *
 *  Meaning of symbols:
 *  i:      input
 *  o:      output
 *  e:      standard error output
 *  x:      expression
 *  args:   a list of *evaluated* arguments
 *  ne:     a newly created expression
 *
 *  @todo Better error reporting, report() should be able to
 *        (optionally) print out expressions
 *  @todo Better error handling; a new primitive type should be made
 *        for it, one that can be caught.
 *  @todo Make the special forms less special!
 *  @todo Make more primitives and mechanisms for handling things:
 *         - Register internal functions as lisp primitives.
 *         - random, seed
 *         - cons,listlen,reverse, more advanced lisp manipulation funcs ...
 *         - eq = > < <= >=
 *         - string manipulation and regexes; tr, sed, //m, pack, unpack, ...
 *         - type? <- returns type of expr
 *         - type coercion and casting
 *         - file manipulation and i/o: read, format, 
 *           read-char read-line write-string, ...
 *         - max, min, abs, ...
 *         - Error handling and recovery
 *         - not, and, or, logical functions as well!
 *         - comment; instead of normal comments, comments and the
 *         unevaluated sexpression could be stored for later retrieval
 *         and inspection, keeping the source and the runnning program
 *         united.
 *         - modules; keywords for helping in the creation of modules
 *         and importing them.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type.h"
#include "io.h"
#include "mem.h"
#include "sexpr.h"
#include "lisp.h"

/** macro helpers **/
#define car(X)      ((X)->data.list[0])
#define cdr(X)      ((X)->data.list[1])
#define cddr(X)     ((X)->data.list[2])
#define cdddr(X)    ((X)->data.list[3])
#define nth(X,Y)    ((X)->data.list[(Y)])
#define tstlen(X,Y) ((Y)==((X)->len))

/** global-to-file special objects **/
static expr nil, tee;

/** functions necessary for the intepreter **/
static expr mkobj(sexpr_e type, io *e);
static expr mksym(char *s, io *e);
static expr mkprimop(expr (*func)(expr args, lisp l),io *e);
static expr evlis(expr x,expr env,lisp l);
static expr apply(expr proc, expr args, expr env, lisp l);
static expr find(expr env, expr x, io *e);
static expr extend(expr sym, expr val, lisp l);
static bool primcmp(expr x, const char *s, io *e);

/** built in primitives **/
static expr primop_fake(expr args, lisp l); /* dummy for certain special forms */
static expr primop_add(expr args, lisp l);
static expr primop_sub(expr args, lisp l);
static expr primop_prod(expr args, lisp l);
static expr primop_div(expr args, lisp l);
static expr primop_cdr(expr args, lisp l);
static expr primop_car(expr args, lisp l);
static expr primop_cons(expr args, lisp l);

/*** interface functions *****************************************************/

/**
 *  @brief          Initialize the lisp interpreter
 *  @param          void
 *  @return         A fully initialized lisp environment
 **/
lisp initlisp(void){
  lisp l;
  expr global;
  l      = wcalloc(1,sizeof (lispenv_t),NULL);
  global = wcalloc(1,sizeof (sexpr_t),NULL);
  if(!l||!global)
    exit(-1);

  /* set up file I/O and pointers */
  l->i.type     = file_in;
  l->i.ptr.file = stdin;
  l->o.type     = file_out;
  l->o.ptr.file = stdout;
  l->e.type     = file_out ;
  l->e.ptr.file = stderr;
  l->current    = NULL;
  l->global     = global;

  global->type  = S_LIST;
  nil = mkobj(S_NIL,&l->e);
  tee = mkobj(S_TEE,&l->e);

  /* internal symbols */
  extend(mksym("nil", &l->e),nil,l);
  extend(mksym("t", &l->e),tee,l);

  /* special forms */
  extend(mksym("begin", &l->e),mkprimop(primop_fake,&l->e),l);
  extend(mksym("if",    &l->e),mkprimop(primop_fake,&l->e),l);
  extend(mksym("quote", &l->e),mkprimop(primop_fake,&l->e),l);
  extend(mksym("set",   &l->e),mkprimop(primop_fake,&l->e),l);
  extend(mksym("define",&l->e),mkprimop(primop_fake,&l->e),l);
  extend(mksym("lambda",&l->e),mkprimop(primop_fake,&l->e),l);

  /* normal forms, kind of  */
  extend(mksym("add",&l->e),mkprimop(primop_add,&l->e),l);
  extend(mksym("sub",&l->e),mkprimop(primop_sub,&l->e),l);
  extend(mksym("mul",&l->e),mkprimop(primop_prod,&l->e),l);
  extend(mksym("div",&l->e),mkprimop(primop_div,&l->e),l);
  extend(mksym("car",&l->e),mkprimop(primop_car,&l->e),l);
  extend(mksym("cdr",&l->e),mkprimop(primop_cdr,&l->e),l);
  extend(mksym("cons",&l->e),mkprimop(primop_cons,&l->e),l);

  return l;
}

/**
 *  @brief          Evaluate an already parsed lisp expression
 *  @param          x   The s-expression to parse
 *  @param          env The environment to evaluate in
 *  @param          l   The global lisp environment
 *  @return         An evaluate expression, possibly ready for printing.
 **/
expr eval(expr x, expr env, lisp l){
  unsigned int i;
  io *e = &l->e;

  if(NULL==x){
    report("passed null!");
    abort();
  }

  switch(x->type){
    case S_LIST:
      if(tstlen(x,0)) /* () */
        return nil;
      if(S_SYMBOL==car(x)->type){
        if(primcmp(x,"if",e)){ /* (if test conseq alt) */
          if(!tstlen(x,4)){
            report("if: argc != 4");
            return nil;
          }
          if(nil == eval(cdr(x),env,l)){
            return eval(cdddr(x),env,l);
          } else {
            return eval(cddr(x),env,l);
          }
        } else if (primcmp(x,"begin",e)){ /* (begin exp ... ) */
          if(tstlen(x,1)){
            return nil;
          }
          for (i = 1; i < x->len - 1; i++){
            eval(nth(x,i),env,l);
          }
          return eval(nth(x,i),env,l);
        } else if (primcmp(x,"quote",e)){ /* (quote exp) */
          if(!tstlen(x,2)){
            report("quote: argc != 1");
            return nil;
          }
          return cdr(x);
        } else if (primcmp(x,"set",e)){
          expr ne;
          if(!tstlen(x,3)){
            report("set: argc != 2");
            return nil;
          }
          ne = find(l->global,cdr(x),&l->e);
          if(nil == ne){
            return nil;
          }
          ne->data.list[1] = eval(cddr(x),env,l);
          return cdr(ne);
        } else if (primcmp(x,"define",e)){ /*what to do if already defined?*/
          if(!tstlen(x,3)){
            report("define: argc != 2");
            return nil;
          }
          return extend(cdr(x),eval(cddr(x),env,l),l);
        } else if (primcmp(x,"lambda",e)){
        } else {
          return apply(eval(car(x),env,l),evlis(x,env,l),env,l);
        }
      } else {
        report("cannot apply");
        print_expr(car(x),&l->o,0,e);
      }
      break;
    case S_SYMBOL:/*if symbol found, return it, else error; unbound symbol*/
      {
        expr ne = find(l->global,x,&l->e);
        if(nil == ne){
          return nil;
        }
        return cdr(find(l->global,x,&l->e));
      }
    case S_FILE: /* to implement */
      report("file type unimplemented");
      return nil;
    case S_NIL:
    case S_TEE:
    case S_STRING:
    case S_PROC:
    case S_INTEGER:
    case S_PRIMITIVE:
      return x;
    default:
      report("Serious error, unknown type"); 
      abort();
  }

  report("should never get here");
  return x;
}

/*** internal functions ******************************************************/

/** find a symbol in a special type of list **/
static expr find(expr env, expr x, io *e){
  unsigned int i;
  char *s = x->data.symbol;
  for(i = 0; i < env->len; i++){
    if(!strcmp(car(nth(env,i))->data.symbol, s)){
      return nth(env,i);
    }
  }
  report("unbound symbol");
  return nil;
}

/** extend the global lisp environment **/
static expr extend(expr sym, expr val, lisp l){
  expr ne = mkobj(S_LIST,&l->e);
  append(ne,sym,&l->e);
  append(ne,val,&l->e);
  append(l->global,ne,&l->e);
  return val;
}

/** make new object **/
static expr mkobj(sexpr_e type,io *e){
  expr x;
  x = wcalloc(sizeof(sexpr_t), 1,e);
  x->len = 0;
  x->type = type;
  return x;
}

/** make a new symbol **/
static expr mksym(char *s,io *e){
  expr x;
  x = mkobj(S_SYMBOL,e);
  x->len = strlen(s);
  x->data.symbol = s;
  return x;
}

/** make a new primitive **/
static expr mkprimop(expr (*func)(expr args, lisp l),io *e){
  expr x;
  x = mkobj(S_PRIMITIVE,e);
  x->data.func = func;
  return x;
}

/** a fake placeholder function for special forms **/
static expr primop_fake(expr args, lisp l){
  io *e = &l->e;
  report("This is a place holder, you should never get here");
  print_expr(args,&l->o,0,&l->e);
  return nil;
}

/** compare a symbols name to a string **/
static bool primcmp(expr x, const char *s, io *e){
  if(NULL == (car(x)->data.symbol)){
    report("null passed to primcmp!");
    abort();
  }
  return !strcmp(car(x)->data.symbol,s);
}

static expr evlis(expr x,expr env,lisp l){
  unsigned int i;
  expr ne;
  ne = mkobj(S_LIST,&l->e);
  for(i = 1/*skip 0!*/; i < x->len; i++){/** @todo change so it does not use append!*/
    append(ne,eval(nth(x,i),env,l),&l->e);
  }
  return ne;
}

static expr apply(expr proc, expr args, expr env, lisp l){
  io *e = &l->e;
  if(S_PRIMITIVE == proc->type){
    return (proc->data.func)(args,l);
  }
  if(S_PROC == proc->type){
  }

  report("Cannot apply expression");
  return nil;
}


/*** primitive operations ****************************************************/

static expr primop_cons(expr args, lisp l){
  io *e = &l->e;
  expr ne = mkobj(S_LIST,e);
  if(2!=args->len){
    report("cons: argc != 2");
    return nil;
  }
  return ne;
}

static expr primop_car(expr args, lisp l){
  io *e = &l->e;
  expr ne = car(args);
  if(S_LIST != ne->type){
    report("args != list");
    return nil;
  }
  if(1!=args->len){
    report("car: argc != 1");
    return nil;
  }
  return car(ne);
}

static expr primop_cdr(expr args, lisp l){
  io *e = &l->e;
  expr ne = mkobj(S_LIST,e), carg = car(args);
  if((S_LIST != carg->type) || (1>=carg->len)){
    return nil;
  }
  ne->data.list = carg->data.list+1;
  ne->len = carg->len - 1;
  return ne;
}

static expr primop_add(expr args, lisp l){
  io *e = &l->e;
  unsigned int i;
  expr ne;
  ne = mkobj(S_INTEGER,e);
  if(0 == args->len)
    return nil;
  for(i = 0; i < args->len; i++){
    if(S_INTEGER!=nth(args,i)->type){
      report("not an integer type"); 
      return nil;
    }
    ne->data.integer+=(nth(args,i)->data.integer);
  }
  return ne;
}

static expr primop_prod(expr args, lisp l){
  io *e = &l->e;
  unsigned int i;
  expr ne;
  ne = mkobj(S_INTEGER,e);
  if(0 == args->len)
    return nil;
  ne = nth(args,0);
  for(i = 1; i < args->len; i++){
    if(S_INTEGER!=nth(args,i)->type){
      report("not an integer type"); 
      return nil;
    }
    ne->data.integer*=(nth(args,i)->data.integer);
  }
  return ne;
}

static expr primop_sub(expr args, lisp l){
  io *e = &l->e;
  unsigned int i;
  expr ne;
  ne = mkobj(S_INTEGER,e);
  if(0 == args->len)
    return nil;
  ne = nth(args,0);
  for(i = 1; i < args->len; i++){
    if(S_INTEGER!=nth(args,i)->type){
      report("not an integer type"); 
      return nil;
    }
    ne->data.integer-=(nth(args,i)->data.integer);
  }
  return ne;
}

static expr primop_div(expr args, lisp l){
  io *e = &l->e;
  unsigned int i,tmp;
  expr ne;
  ne = mkobj(S_INTEGER,e);
  if(0 == args->len)
    return nil;
  ne = nth(args,0);
  for(i = 1; i < args->len; i++){
    if(S_INTEGER!=nth(args,i)->type){
      report("not an integer type"); 
      return nil;
    }
    tmp = nth(args,i)->data.integer;
    if(tmp){
      ne->data.integer/=tmp;
    }else{
      report("attempted /0");
      return nil;
    }
  }
  return ne;
}

/*****************************************************************************/
