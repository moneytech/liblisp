/**
 *  @file           sexpr.h
 *  @brief          General S-Expression parsing interface, header
 *  @author         Richard James Howe.
 *  @copyright      Copyright 2013 Richard James Howe.
 *  @license        GPL v2.0 or later version
 *  @email          howe.r.j.89@gmail.com
 **/

#ifndef SEXPR_H
#define SEXPR_H

/**** macros ******************************************************************/
#define sexpr_perror(EXP,MSG,E)  dosexpr_perror((EXP),(MSG),__FILE__,__LINE__,(E))
/******************************************************************************/

/**** function prototypes *****************************************************/
void set_color_on(bool flag);
void set_print_proc(bool flag);
expr sexpr_parse(io *i, io *e);
void sexpr_print(expr x, io *o, unsigned int depth, io *e);
void dosexpr_perror(expr x, char *msg, char *cfile, unsigned int linenum, io *e);
void append(expr list, expr ele, io *e);
/******************************************************************************/

#endif
