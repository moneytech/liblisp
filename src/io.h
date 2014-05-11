/**
 *  @file           io.h
 *  @brief          I/O redirection and wrappers, header
 *  @author         Richard James Howe.
 *  @copyright      Copyright 2013 Richard James Howe.
 *  @license        GPL v3.0
 *  @email          howe.r.j.89@gmail.com
 **/

#ifndef IO_H
#define IO_H

/**** macros ******************************************************************/
#define BUFLEN      (256u)
#define report(X)   doreport(X,__FILE__,__LINE__,e)
#define NULLCHK(X)  if(NULL == (X))\
                      { report("null dereference"); exit(EXIT_FAILURE);}
/******************************************************************************/

/**** function prototypes *****************************************************/
int wputc(char c, io *o, io *e);
int wgetc(io *i, io *e);
int wungetc(char c, io *i, io *e);
int wprintd(cell_t d, io *o, io *e);
int wprints(const char *s, io *o , io *e); /** error code?*/
void doreport(const char *s, char *cfile, unsigned int linenum, io *e);
/******************************************************************************/
#endif
