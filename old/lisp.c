/* 
 * Richard James Howe
 * Howe Lisp.
 *
 * Lisp interpreter.
 *
 * @author         Richard James Howe.
 * @copyright      Copyright 2013 Richard James Howe.
 * @license        LGPL      
 * @email          howe.r.j.89@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lisp.h"

/*****************************************************************************/
/*basic io*/
static int wrap_get(file_io_t * in);
static int wrap_put(file_io_t * out, char c);
static void wrap_ungetc(file_io_t * in, char c);
static void print_string(char *s, file_io_t * out);
static void print_error(char *s, file_io_t * err, char *file, unsigned int linenum);
/*parser*/
static cell_t *parse_number(file_io_t * in, file_io_t * err);
static cell_t *parse_string(file_io_t * in, file_io_t * err);
static cell_t *parse_symbol(file_io_t * in, file_io_t * err);
static int append(cell_t * head, cell_t * child, file_io_t * err);
static cell_t *parse_list(file_io_t * in, file_io_t * err);
static void print_space(int depth, file_io_t * out);
/*internal lisp*/

static int add_primitive_to_dictionary(char *s, lenv_t * le,
                             int (*function)(void *p));

/*****************************************************************************/
/*Input / output wrappers.*/

static int wrap_get(file_io_t * in)
{
        char tmp;
        if (in->ungetc_flag == true) {
                in->ungetc_flag = false;
                return in->ungetc_char;
        }

        switch (in->fiot) {
        case io_stdin:
                return getc(stdin);
        case io_rd_file:
                if (in->io_ptr.f != NULL)
                        return fgetc(in->io_ptr.f);
                else
                        return EOF;
        case io_rd_str:
                if (in->io_ptr.s != NULL) {
                        if (in->str_index > in->str_max_len)
                                return EOF;
                        tmp = in->io_ptr.s[++in->str_index];
                        if (tmp != '\0')
                                return (int)tmp;
                        else
                                return EOF;
                } else
                        return EOF;
        default:
                return EOF;
        }
}

static int wrap_put(file_io_t * out, char c)
{
        switch (out->fiot) {
        case io_stdout:
                return putc(c, stdout);
        case io_stderr:
                return putc(c, stderr);
        case io_wr_file:
                if (out->io_ptr.f != NULL)
                        return fputc(c, out->io_ptr.f);
                else
                        return EOF;
        case io_wr_str:
                if (out->io_ptr.s != NULL) {
                        if (out->str_index > out->str_max_len)
                                return EOF;
                        out->io_ptr.s[++(out->str_index)] = c;
                } else {
                        return EOF;
                }
                return ERR_OK;
        default:
                return EOF;
        }
}

static void wrap_ungetc(file_io_t * in, char c)
{
        in->ungetc_flag = true;
        in->ungetc_char = c;
        return;
}

static void print_string(char *s, file_io_t * out)
{
        int i;
        for (i = 0; s[i] != '\0'; i++)
                (void)wrap_put(out, s[i]);
}

static void print_error(char *s, file_io_t * err, char *file, unsigned int linenum)
{
        /*fprintf(stderr, "(error \"%s\" \"%s\" %d)\n",(X),__FILE__,__LINE__); */
        print_string("(error \"", err);
        print_string(s, err);
        print_string("\" " __FILE__ ")", err);
        /*PRINT LINE NUMBER */
        (void)wrap_put(err, '\n');
}

/*****************************************************************************/

/*parse a number*/
static cell_t *parse_number(file_io_t * in, file_io_t * err)
{
        char buf[MAX_STR] = { 0 };
        int i = 0, c;
        cell_t *cell_num = calloc(1, sizeof(cell_t));
        if (cell_num == NULL) {
                print_error("calloc() failed", err, __FILE__,__LINE__);
                return NULL;
        }
        while ((c = wrap_get(in)) != EOF) {
                if (i >= MAX_STR) {     /*This should instead be set the maximum length of a number-string */
                        print_error("String too long for a number.", err, __FILE__,__LINE__);
                        goto FAIL;
                }

                if (isdigit(c)) {
                        buf[i] = c;
                        i++;
                } else if (c == '(' || c == ')' || c == '"') {
                        wrap_ungetc(in, c);
                        goto SUCCESS;
                } else if (isspace(c)) {
                        goto SUCCESS;
                } else {
                        print_error("Not a valid digit.", err, __FILE__,__LINE__);
                        goto FAIL;
                }
        }
 SUCCESS:
        cell_num->type = type_number;
        cell_num->car.i = atoi(buf);
        cell_num->cdr.cell = NULL;      /*stating this explicitly. */
        return cell_num;
 FAIL:
        print_error("parsing number failed.", err, __FILE__,__LINE__);
        free(cell_num);
        return NULL;
}

/*parse a string*/
static cell_t *parse_string(file_io_t * in, file_io_t * err)
{
        char buf[MAX_STR];
        int i = 0, c;
        cell_t *cell_str = calloc(1, sizeof(cell_t));
        if (cell_str == NULL) {
                print_error("calloc() failed", err, __FILE__,__LINE__);
                return NULL;
        }

        while ((c = wrap_get(in)) != EOF) {
                if (i >= MAX_STR) {
                        print_error("String too long.", err, __FILE__,__LINE__);
                        goto FAIL;
                }

                switch (c) {
                case '"':      /*success */
                        cell_str->type = type_str;
                        /*could add in string length here. */
                        cell_str->car.s = calloc(i + 1, sizeof(char));
                        if (cell_str->car.s == NULL) {
                                print_error("calloc() failed", err, __FILE__,__LINE__);
                                goto FAIL;
                        }
                        memcpy(cell_str->car.s, buf, i);
                        cell_str->cdr.cell = NULL;      /*stating this explicitly. */
                        return cell_str;
                case '\\':     /*escape char */
                        switch (c = wrap_get(in)) {
                        case '\\':     /*fall through */
                        case '"':
                                buf[i] = c;
                                i++;
                                continue;
                        case 'n':
                                buf[i] = '\n';
                                i++;
                                continue;
                        case EOF:
                                print_error
                                    ("EOF encountered while processing escape char",
                                     err, __FILE__,__LINE__);
                                goto FAIL;
                        default:
                                print_error("Not an escape character", err, __FILE__,__LINE__);
                                goto FAIL;
                        }
                case EOF:
                        print_error("EOF encountered while processing symbol",
                                    err, __FILE__,__LINE__);
                        goto FAIL;
                default:       /*just add the character into the buffer */
                        buf[i] = c;
                        i++;
                }
        }
 FAIL:
        print_error("parsing string failed.", err, __FILE__,__LINE__);
        free(cell_str);
        return NULL;
}

/*parse a symbol*/
static cell_t *parse_symbol(file_io_t * in, file_io_t * err)
{
        char buf[MAX_STR];
        int i = 0, c;
        cell_t *cell_sym = calloc(1, sizeof(cell_t));
        if (cell_sym == NULL) {
                print_error("calloc() failed", err, __FILE__,__LINE__);
                return NULL;
        }

        while ((c = wrap_get(in)) != EOF) {
                if (i >= MAX_STR) {
                        print_error("String (symbol) too long.", err, __FILE__,__LINE__);
                        goto FAIL;
                }
                if (isspace(c))
                        goto SUCCESS;

                if (c == '(' || c == ')') {
                        wrap_ungetc(in, c);
                        goto SUCCESS;
                }

                switch (c) {
                case '\\':
                        switch (c = wrap_get(in)) {
                        case '"':
                        case '(':
                        case ')':
                                buf[i] = c;
                                i++;
                                continue;
                        default:
                                print_error("Not an escape character", err, __FILE__,__LINE__);
                                goto FAIL;
                        }
                case '"':
                        print_error
                            ("Unescaped \" or incorrectly formatted input.",
                             err, __FILE__,__LINE__);
                        goto FAIL;
                case EOF:
                        print_error("EOF encountered while processing symbol",
                                    err, __FILE__,__LINE__);
                        goto FAIL;
                default:       /*just add the character into the buffer */
                        buf[i] = c;
                        i++;
                }
        }
 SUCCESS:
        cell_sym->car.s = calloc(i + 1, sizeof(char));
        if (cell_sym->car.s == NULL) {
                print_error("calloc() failed", err, __FILE__,__LINE__);
                goto FAIL;
        }
        cell_sym->type = type_symbol;
        memcpy(cell_sym->car.s, buf, i);
        cell_sym->cdr.cell = NULL;      /*stating this explicitly. */
        return cell_sym;
 FAIL:
        print_error("parsing symbol failed.", err, __FILE__,__LINE__);
        free(cell_sym);
        return NULL;
}

static int append(cell_t * head, cell_t * child, file_io_t * err)
{
        if (head == NULL || child == NULL) {
                print_error("append failed, head or child is null", err, __FILE__,__LINE__);
                return 1;
        }
        head->cdr.cell = child;
        return 0;
}

static cell_t *parse_list(file_io_t * in, file_io_t * err)
{
        cell_t *head, *child = NULL, *cell_lst = calloc(1, sizeof(cell_t));
        int c;
        head = cell_lst;
        if (cell_lst == NULL) {
                print_error("calloc() failed", err, __FILE__,__LINE__);
                return NULL;
        }
        while ((c = wrap_get(in)) != EOF) {
                if (isspace(c))
                        continue;

                if (isdigit(c)) {
                        wrap_ungetc(in, c);
                        child = parse_number(in, err);
                        if (append(head, child, err))
                                goto FAIL;
                        head = child;
                        continue;
                }

                switch (c) {
                case ')':      /*finished */
                        goto SUCCESS;
                case '(':      /*encountered a nested list */
                        child = calloc(1, sizeof(cell_t));
                        if (child == NULL)
                                goto FAIL;
                        head->cdr.cell = child;
                        child->car.cell = parse_list(in, err);
                        if (child->car.cell == NULL)
                                goto FAIL;
                        child->type = type_list;
                        head = child;
                        break;
                case '"':      /*parse string */
                        child = parse_string(in, err);
                        if (append(head, child, err))
                                goto FAIL;
                        head = child;
                        break;
                case EOF:      /*Failed */
                        print_error("EOF occured before end of list did.", err, __FILE__,__LINE__);
                        goto FAIL;
                default:       /*parse symbol */
                        wrap_ungetc(in, c);
                        child = parse_symbol(in, err);
                        if (append(head, child, err))
                                goto FAIL;
                        head = child;
                        break;
                }

        }
 SUCCESS:
        cell_lst->type = type_list;
        return cell_lst;
 FAIL:
        print_error("parsing list failed.", err, __FILE__,__LINE__);
        free(cell_lst);
        return NULL;
}

cell_t *parse_sexpr(file_io_t * in, file_io_t * err)
{
        int c;
        if (in != NULL)
                while ((c = wrap_get(in)) != '\0') {
                        if (isspace(c))
                                continue;

                        if (isdigit(c)) {
                                wrap_ungetc(in, c);
                                return parse_number(in, err);
                        }

                        switch (c) {
                        case '(':
                                return parse_list(in, err);
                        case '"':
                                return parse_string(in, err);
                        case EOF:
                                print_error("EOF, nothing to parse", err, __FILE__,__LINE__);
                                return NULL;
                        case ')':
                                print_error("Unmatched ')'", err, __FILE__,__LINE__);
                                return NULL;
                        default:
                                wrap_ungetc(in, c);
                                return parse_symbol(in, err);
                        }

                }

        print_error("parse_expr in == NULL", err, __FILE__,__LINE__);
        return NULL;
}

static void print_space(int depth, file_io_t * out)
{
        int i;
        for (i = 0; i < ((depth * 2)); i++)
                (void)wrap_put(out, ' ');
}

void print_sexpr(cell_t * list, int depth, file_io_t * out, file_io_t * err)
{
        char buf[MAX_STR];
        cell_t *tmp;
        if (list == NULL) {
                print_error("print_sexpr was passed a NULL!", err, __FILE__,__LINE__);
                return;
        }

        if (list->type == type_null) {
                print_space(depth + 1, out);
                print_string("Null\n", out);
                return;
        } else if (list->type == type_number) {
                print_space(depth + 1, out);
                sprintf(buf, "%d", list->car.i);
                print_string(buf, out);
                wrap_put(out, '\n');
        } else if (list->type == type_str) {
                print_space(depth + 1, out);
                wrap_put(out, '"');
                print_string(list->car.s, out);
                wrap_put(out, '"');
                wrap_put(out, '\n');
                return;
        } else if (list->type == type_symbol) {
                print_space(depth + 1, out);
                print_string(list->car.s, out);
                wrap_put(out, '\n');
                return;
        } else if (list->type == type_list) {
                if (depth == 0) {
                        print_string("(\n", out);
                }
                for (tmp = list; tmp != NULL; tmp = tmp->cdr.cell) {
                        if (tmp->car.cell != NULL && tmp->type == type_list) {
                                print_space(depth + 1, out);
                                print_string("(\n", out);

                                print_sexpr(tmp->car.cell, depth + 1, out, err);

                                print_space(depth + 1, out);
                                print_string(")\n", out);
                        }

                        if (tmp->type != type_list) {
                                print_sexpr(tmp, depth + 1, out, err);
                        }
                }
                if (depth == 0) {
                        print_string(")\n", out);
                }
        }

        return;
}

void free_sexpr(cell_t * list, file_io_t * err)
{
        cell_t *tmp, *free_me;
        if (list == NULL)
                return;
        if (list->type == type_number) {
                /*do not need to do anything */
        } else if (list->type == type_str) {
                free(list->car.s);
                return;
        } else if (list->type == type_symbol) {
                free(list->car.s);
                return;
        } else if (list->type == type_list) {
                for (tmp = list; tmp != NULL;) {
                        if (tmp->car.cell != NULL && tmp->type == type_list) {
                                free_sexpr(tmp->car.cell, err);
                        }

                        if (tmp->type != type_list) {
                                free_sexpr(tmp, err);
                        }

                        free_me = tmp;
                        tmp = tmp->cdr.cell;
                        free(free_me);
                }
                return;
        }

        return;
}

/*******************************************************************************/
/***PRIMITIVES!*****************************************************************/
/*******************************************************************************/

int prim_null(void *p){
  lenv_t *le = p;
  print_error("It appears this primitive has not been implemented yet...", le->err, __FILE__,__LINE__);
  return 0;
}

lenv_t *init_lisp(void)
{
        lenv_t *le = calloc(1, sizeof(lenv_t));
        if (le == NULL)
                return NULL;

        le->in = calloc(1, sizeof(file_io_t));
        le->out = calloc(1, sizeof(file_io_t));
        le->err = calloc(1, sizeof(file_io_t));
        if (le->in == NULL || le->out == NULL || le->err == NULL) {
                free(le);
                return NULL;
        }

        le->return_code = ERR_OK;

        /*Set up io */
        le->in->fiot = io_stdin;
        le->in->ungetc_flag = 0;
        le->in->ungetc_char = '\0';

        le->out->fiot = io_stdout;
        le->out->ungetc_flag = 0;
        le->out->ungetc_char = '\0';

        le->err->fiot = io_stderr;
        le->err->ungetc_flag = 0;
        le->err->ungetc_char = '\0';

        /*Allocating stack */
        le->variable_stack = calloc(STK_SIZ, sizeof(cell_t));

        /*creating initial dictionary entry */
        le->dictionary = calloc(1, sizeof(cell_t));
        if (le->dictionary == NULL) {
                free(le->in);
                free(le->out);
                free(le->err);
                free(le);
                return NULL;
        }
        le->dictionary_tail = le->dictionary;
        le->dictionary->type = type_list;       
        le->current_expression = NULL; 

      /*put things in the dictionary*/ 
        add_primitive_to_dictionary("quote",le,prim_null);
        add_primitive_to_dictionary("atom",le,prim_null);
        add_primitive_to_dictionary("eq",le,prim_null);
        add_primitive_to_dictionary("cons",le,prim_null);
        add_primitive_to_dictionary("cond",le,prim_null);
        add_primitive_to_dictionary("car",le,prim_null);
        add_primitive_to_dictionary("cdr",le,prim_null);
        add_primitive_to_dictionary("=",le,prim_null);
        add_primitive_to_dictionary(">",le,prim_null);
        add_primitive_to_dictionary("<",le,prim_null);
        add_primitive_to_dictionary("+",le,prim_null);
        add_primitive_to_dictionary("-",le,prim_null);
        add_primitive_to_dictionary("*",le,prim_null);
        add_primitive_to_dictionary("and",le,prim_null);
        add_primitive_to_dictionary("not",le,prim_null);
        add_primitive_to_dictionary("or",le,prim_null);
        add_primitive_to_dictionary("for",le,prim_null);    /*for loop*/
        add_primitive_to_dictionary("define",le,prim_null);

        return le;
}

/*define static*/
int add_primitive_to_dictionary(char *s, lenv_t *le,
                             int (*function)(void *p))
{
        /*add_me refers to the content, char *s refers to the symbol to refer that
         *content by*/
        cell_t *tmp, *add_me;
        if ((tmp = calloc(1, sizeof(cell_t))) == NULL)
                return ERR_MALLOC;

        if ((add_me = calloc(1, sizeof(cell_t))) == NULL)
                return ERR_MALLOC;

        tmp->type = type_list;
        le->dictionary_tail->cdr.cell = tmp;
        le->dictionary_tail = tmp;

        tmp->car.cell = add_me;
        add_me->type = type_function;
        add_me->car.s = s;
        add_me->cdr.function = function;

        return ERR_OK;
}

/*define static, should check for NULLs*/
cell_t *find_symbol_in_dictionary(char *s, cell_t * dictionary)
{
        cell_t *cur;
        /*TODO: fix dictionary->cdr.cell hack job*/
        for (cur = dictionary->cdr.cell; cur != NULL; cur = cur->cdr.cell) {
                if (!strcmp(s, (cur->car.cell)->car.s))
                        return cur->car.cell;
        }
        return NULL;
}

cell_t *evaluate_expr(lenv_t * le, int depth, cell_t * list)
{
        char buf[MAX_STR];
        cell_t * tmp, *retn;
        le->current_expression = list;
        if (le == NULL || list == NULL)
                return NULL;

        if (list == NULL) {
        }
        if (list->type == type_number) {
                /*just print out a number */
                sprintf(buf, "%d", list->car.i);
                print_string(buf, le->out);
                wrap_put(le->out, '\n');
        } else if (list->type == type_str) {
                /*print out string */
                wrap_put(le->out, '"');
                print_string(list->car.s, le->out);
                wrap_put(le->out, '"');
                wrap_put(le->out, '\n');
        } else if (list->type == type_symbol) {
                /*find */
              if((retn=find_symbol_in_dictionary(list->car.s, le->dictionary))!=NULL){
                retn->cdr.function(le);
              } else {
                print_error("Symbol not found in dictionary",le->err, __FILE__,__LINE__);
              }
        } else if (list->type == type_list) {
                for (tmp = list; tmp != NULL;) {
                        if (tmp->car.cell != NULL && tmp->type == type_list) {
                          evaluate_expr(le,depth+1,tmp->car.cell);
                        }

                        if (tmp->type != type_list) {
                          evaluate_expr(le,depth+1,tmp);
                        }

                        tmp = tmp->cdr.cell;
                }
                return NULL;
        }

        return NULL;
}

lenv_t *lisp(lenv_t * le)
{
        cell_t *tmp;

        if (le == NULL) {
                le = init_lisp();
                if (le == NULL) {
                        return NULL;
                }
        }
        while (true) {
                /*
                 * Interpreter outline:
                 *  Read/Parse expression
                 *  Evaluate expression
                 *    - Look up symbols in table (X ... ), X should be a symbol
                 *      - Dictionary of primitives/expressions
                 *    - Single variable stack
                 *  Print anything necessary
                 *  Loop
                 *
                 */
                tmp = parse_sexpr(le->in, le->err);
                if (tmp == NULL)
                        return le;
                print_sexpr(tmp, 0, le->out, le->err);
                evaluate_expr(le,0,tmp);
                /*free will not be performed in final program*/
                /*free_sexpr(tmp, le->err);*/
        }
        return le;
}

int destroy_lisp(lenv_t * le)
{
  print_error("It appears this function has not been implemented yet...", le->err, __FILE__,__LINE__);
  return 0;
}