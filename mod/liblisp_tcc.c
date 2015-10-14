/** @file       liblisp_tcc.c
 *  @brief      Tiny C Compiler liblisp module
 *  @author     Richard Howe (2015)
 *  @license    LGPL v2.1 or Later 
 *              <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html> 
 *  @email      howe.r.j.89@gmail.com**/
#include <assert.h>
#include <libtcc.h>
#include <liblisp.h>
#include <stdlib.h>
#include <stdint.h>

#define SUBROUTINE_XLIST\
        X(subr_compile,             "compile")\
        X(subr_link,                "link-library")\
        X(subr_compile_file,        "compile-file")\
        X(subr_get_subr,            "get-subroutine")\
        X(subr_add_include_path,    "add-include-path")\
        X(subr_add_sysinclude_path, "add-system-include-path")\
        X(subr_set_lib_path,        "set-library-path")\

#define X(SUBR, NAME) static cell* SUBR (lisp *l, cell *args);
SUBROUTINE_XLIST /*function prototypes for all of the built-in subroutines*/
#undef X

#define X(SUBR, NAME) { SUBR, NAME },
static struct module_subroutines { subr p; char *name; } primitives[] = {
        SUBROUTINE_XLIST /*all of the subr functions*/
        {NULL, NULL} /*must be terminated with NULLs*/
};
#undef X

static int ud_tcc = 0;

static void ud_tcc_free(cell *f) {
        tcc_delete(get_user(f));
        free(f);
}

static int ud_tcc_print(io *o, unsigned depth, cell *f) {
        return lisp_printf(NULL, o, depth, "%B<COMPILE-STATE:%d>%t", get_user(f));
}

static cell* subr_compile(lisp *l, cell *args) {
        if(!cklen(args, 3) 
        || !is_usertype(car(args), ud_tcc)
        || !is_asciiz(CADR(args)) || !is_str(CADDR(args)))
                RECOVER(l, "\"expected (compile-state string string\" '%S", args);
        char *fname = get_str(CADR(args)), *prog = get_str(CADDR(args));
        subr func;
        TCCState *st = get_user(car(args));
        if(tcc_compile_string(st, prog) < 0)
                return gsym_error();
        if(tcc_relocate(st, TCC_RELOCATE_AUTO) < 0)
                return gsym_error();
        func = (subr)tcc_get_symbol(st, fname);
        return mk_subr(l, func, NULL, NULL);
}

static cell* subr_link(lisp *l, cell *args) {
        if(!cklen(args, 2) 
        || !is_usertype(car(args), ud_tcc) || !is_asciiz(CADR(args)))
                RECOVER(l, "\"expected (compile-state string)\" '%S", args);
        return tcc_add_library(get_user(car(args)), get_str(CADR(args))) < 0 ? gsym_error() : gsym_nil();
}

static cell* subr_compile_file(lisp *l, cell *args) {
        if(!cklen(args, 2)
        || !is_usertype(car(args), ud_tcc) || !is_asciiz(CADR(args)))
                RECOVER(l, "\"expected (compile-state string)\" '%S", args);
        if(tcc_add_file(get_user(car(args)), get_str(CADR(args))) < 0)
                return gsym_error();
        if(tcc_relocate(get_user(car(args)), TCC_RELOCATE_AUTO) < 0)
                return gsym_error();
        return gsym_tee();
}

static cell* subr_get_subr(lisp *l, cell *args) {
        subr func;
        if(!cklen(args, 2)
        || !is_usertype(car(args), ud_tcc) || !is_asciiz(CADR(args)))
                RECOVER(l, "\"expected (compile-state string)\" '%S", args);
        if(!(func = tcc_get_symbol(get_user(car(args)), get_str(CADR(args)))))
                return gsym_error();
        else
                return mk_subr(l, func, NULL, NULL);
}

static cell* subr_add_include_path(lisp *l, cell *args) {
        if(!cklen(args, 2) || !is_usertype(car(args), ud_tcc) || !is_asciiz(CADR(args)))
                RECOVER(l, "\"expected (compile-state string)\" '%S", args);
        return tcc_add_include_path(get_user(car(args)), get_str(CADR(args))) < 0 ? gsym_error() : gsym_tee();
}

static cell* subr_add_sysinclude_path(lisp *l, cell *args) {
        if(!cklen(args, 2) || !is_usertype(car(args), ud_tcc) || !is_asciiz(CADR(args)))
                RECOVER(l, "\"expected (compile-state string)\" '%S", args);
        return tcc_add_sysinclude_path(get_user(car(args)), get_str(CADR(args))) < 0 ? gsym_error() : gsym_tee();
}

static cell* subr_set_lib_path(lisp *l, cell *args) {
        if(!cklen(args, 2) || !is_usertype(car(args), ud_tcc) || !is_asciiz(CADR(args)))
                RECOVER(l, "\"expected (compile-state string)\" '%S", args);
        tcc_set_lib_path(get_user(car(args)), get_str(CADR(args)));
        return gsym_tee();
}

static int initialize(void) {
        size_t i;
        assert(lglobal);
        /** Tiny C compiler library interface, special care has to be taken 
         *  when compiling and linking all of the C files within the liblisp
         *  project so the symbols in it are available to libtcc.
         *
         *  Possible improvements:
         *  * Modification of libtcc so it can accept S-Expressions from
         *    the interpreter. This would be a significant undertaking.
         *  * Add more functions from libtcc
         *  * Separate out tcc_get_symbol from tcc_compile_string
         *  * Find out why link does not work
         **/
        ud_tcc = new_user_defined_type(lglobal, ud_tcc_free, NULL, NULL, ud_tcc_print);
        if(ud_tcc < 0)
                goto fail;
        TCCState *st = tcc_new();
        tcc_set_output_type(st, TCC_OUTPUT_MEMORY);
        lisp_add_cell(lglobal, "*compile-state*", mk_user(lglobal, st, ud_tcc));
        for(i = 0; primitives[i].p; i++) /*add all primitives from this module*/
                if(!lisp_add_subr(lglobal, primitives[i].name, primitives[i].p))
                        goto fail;
        lisp_printf(lglobal, lisp_get_logging(lglobal), 0, "module: tcc loaded\n");
        return 0;
fail:   lisp_printf(lglobal, lisp_get_logging(lglobal), 0, "module: tcc load failure\n");
        return -1;
}

#ifdef __unix__
static void construct(void) __attribute__((constructor));
static void destruct(void)  __attribute__((destructor));
static void construct(void) { initialize(); }
static void destruct(void)  { }
#elif _WIN32
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
        switch (fdwReason) {
            case DLL_PROCESS_ATTACH: initialize(); break;
            case DLL_PROCESS_DETACH: break;
            case DLL_THREAD_ATTACH:  break;
            case DLL_THREAD_DETACH:  break;
	    default: break;
        }
        return TRUE;
}
#endif

