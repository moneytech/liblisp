/** @file       private.h
 *  @brief      A *internal* header for the liblisp project, not for external
 *              use. It defines the internals of what are opaque objects to the
 *              programs outside of the library.
 *  @author     Richard Howe (2015)
 *  @license    LGPL v2.1 or Later
 *  @email      howe.r.j.89@gmail.com**/

#ifndef PRIVATE_H
#define PRIVATE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <setjmp.h>
#include <inttypes.h>
#include <stdio.h>

#define DEFAULT_LEN        (256)   /**< just an arbitrary smallish number*/
#define LARGE_DEFAULT_LEN  (4096)  /**< just another arbitrary number*/
#define REGEX_MAX_DEPTH    (8192)  /**< max recursion depth of a regex*/
#define MAX_USER_TYPES     (256)   /**< max number of user defined types*/
#define COLLECTION_POINT   (1<<20) /**< run gc after this many allocs*/
#define UNUSED(X)          ((void)(X)) /**< unused variable*/
#define MAX(X, Y)    ((X)>(Y)?(X):(Y)) /**< largest of two values*/
#define MIN(X, Y)    ((X)>(Y)?(Y):(X)) /**< smallest of two values*/

/**@brief This restores a jmp_buf stored in lisp environment if it
 *        has been copied out to make way for another jmp_buf.
 * @param USED is RBUF used?
 * @param ENV  lisp environment to restore jmp_buf to
 * @param RBUF jmp_buf to restore**/
#define RECOVER_RESTORE(USED, ENV, RBUF)\
        if((USED)) memcpy((ENV)->recover, (RBUF), sizeof(jmp_buf));\
        else (ENV)->recover_init = 0;

typedef enum lisp_type { 
        INVALID, /**< invalid object (default), halts interpreter*/
        SYMBOL,  /**< symbol */
        INTEGER, /**< integer, a normal fixed with number*/
        CONS,    /**< cons cell*/
        PROC,    /**< lambda procedure*/
        SUBR,    /**< subroutine or primitive written in C*/
        STRING,  /**< a NUL terminated string*/
        IO,      /**< Input/Output port*/
        HASH,    /**< Associative hash table*/
        FPROC,   /**< F-Expression*/
        FLOAT,   /**< Floating point number; could be float or double*/
        USERDEF  /**< User defined types*/
} lisp_type;     /**< A lisp object*/

typedef enum trace_level { 
        TRACE_OFF,    /**< tracing is off (default)*/
        TRACE_MARKED, /**< trace only cells that have their trace field set*/
        TRACE_ALL     /**< trace everything*/
} trace_level; /**< level of debug printing to do when evaluating objects*/

typedef enum gc_control { /**< Garbage collection control enumeration*/
        GC_ON,       /**< Garbage collection is on (default)*/
        GC_POSTPONE, /**< Temporarily postpone garbage collection*/
        GC_OFF       /**< *Permanently turn garbage collection off*/
} gc_control; /**< enum for l->gc_state */

typedef union lisp_union { /**< ideally we would use void * for everything*/
        void *v;     /**< use this for integers and points to cells*/
        lfloat f;    /**< if lfloat is double it could be bigger than *v */ 
        subr prim;   /**< function pointers are not guaranteed to fit into a void**/
} lisp_union; /**< a union of all the different C datatypes used*/

struct cell {
        unsigned type:    4,        /**< Type of the lisp object*/
                 userdef: 8,        /**< if type is USERDEF, the this determines which USERDEF*/
                 mark:    1,        /**< mark for garbage collection*/
                 uncollectable: 1,  /**< set if the object cannot, or should not, be freed*/
                 trace:   1,        /**< set if object is to be traced for debugging*/
                 close:   1,        /**< set if the objects data field is now invalid/freed*/
                 len:     32;       /**< length of data p*/
        lisp_union p[1]; /**< uses the "struct hack", c99 does not quite work here*/
}; /**< a tagged object representing all possible lisp objects*/

typedef struct hashentry {      /**< linked list of entries in a bin*/
        char *key;              /**< ASCII nul delimited string*/
        void *val;              /**< arbitrary value*/
        struct hashentry *next; /**< next item in list*/
} hashentry;

struct hashtable {                /**< a hash table*/
        size_t len;               /**< number of 'bins' in the hash table*/
        struct hashentry **table; /**< table of linked lists*/
};

struct io {
        union { FILE *file; char *str; } p; /**< the actual file or string*/
        size_t position, /**< current position, used for string*/
               max;      /**< max position in buffer, used for string*/
        enum { IO_INVALID,    /**< invalid (default)*/ 
               FIN,           /**< file input*/
               FOUT,          /**< file output*/
               SIN,           /**< string input*/
               SOUT,          /**< string output, write to char* block*/
               NULLOUT        /**< null output, discard output*/
        } type; /**< type of the IO object*/
        unsigned ungetc :1, /**< push back is in use?*/
                 color  :1, /**< colorize output? Used in lisp_print*/
                 pretty :1, /**< pretty print output? Used in lisp_print*/
                 eof    :1; /**< End-Of-File marker*/
        char c; /**< one character of push back*/
};

typedef struct gc_list { 
        cell *ref; /**< reference to cell for the garbage collector to act on*/
        struct gc_list *next; /**< next in list*/
} gc_list; /**< type used to form linked list of all allocations*/

typedef struct userdef_funcs { 
        ud_free   free;  /**< to free a user defined type*/
        ud_mark   mark;  /**< to mark a user defined type*/
        ud_equal  equal; /**< to compare two user defined types*/
        ud_print  print; /**< to print two user defined types*/
} userdef_funcs; /**< functions the interpreter uses for user defined types*/

struct lisp {
        jmp_buf recover; /**< jump here when there is an error*/
        io *ifp /**< standard input port*/, 
           *ofp /**< standard output port*/, 
           *efp /**< standard error output port*/;
        cell *all_symbols, /**< all intern'ed symbols*/
             *top_env,     /**< top level lisp environment*/
             **gc_stack;   /**< garbage collection stack for working items*/
        gc_list *gc_head;  /**< linked list of all allocated objects*/
        char *token /**< one token of put back for parser*/, 
             *buf   /**< input buffer for parser*/;
        size_t buf_allocated,   /**< size of buffer "l->buf"*/
               buf_used,        /**< amount of buffer used by current string*/
               gc_stack_allocated,    /**< length of buffer of GC stack*/
               gc_stack_used,         /**< elements used in GC stack*/
               gc_collectp,     /**< collection counter, collect after it goes too high*/
               max_depth        /**< max recursion depth*/;
        uint64_t random_state[2] /**< PRNG state*/;
        int sig;   /**< set by signal handlers or other threads*/
        int trace; /**< trace level for eval*/
        unsigned ungettok:     1, /**< do we have a put-back token to read?*/
                 recover_init: 1, /**< has the recover buffer been initialized?*/
                 dynamic:      1, /**< use lexical scope if false, dynamic if true*/
                 errors_halt:  1, /**< any error halts the interpreter if true*/
                 color_on:     1, /**< REPL Colorize output*/
                 prompt_on:    1, /**< REPL '>' Turn prompt on*/
                 editor_on:    1; /**< REPL Turn the line editor on*/
        gc_control gc_state /**< gc on (default), suspended or permanently off*/;
        userdef_funcs ufuncs[MAX_USER_TYPES]; /**< functions for user defined types*/
        int userdef_used;   /**< number of user defined types allocated*/
        editor_func editor; /**< line editor to use, optional*/
};

#ifdef __cplusplus
}
#endif
#endif