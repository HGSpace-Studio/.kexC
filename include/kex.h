#ifndef KEX_H
#define KEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KEX_VERSION_MAJOR  1
#define KEX_VERSION_MINOR  0
#define KEX_VERSION_PATCH  0

#define KEX_OK             0
#define KEX_ERR           -1
#define KEX_ERR_ARG       -2
#define KEX_ERR_COMPILE   -3
#define KEX_ERR_LINK      -4
#define KEX_ERR_FORMAT    -5
#define KEX_ERR_IO        -6
#define KEX_ERR_NOINPUT   -7

#define KEX_FMT_KEX       0
#define KEX_FMT_KXP       1
#define KEX_FMT_ELF       2

#define KEX_LANG_C        0
#define KEX_LANG_CPP      1

#define KEX_OPT_O0        0
#define KEX_OPT_O1        1
#define KEX_OPT_O2        2
#define KEX_OPT_O3        3
#define KEX_OPT_OS        4

#define KEX_MAX_SOURCES   256
#define KEX_MAX_INC_DIRS  64
#define KEX_MAX_LIB_DIRS  64
#define KEX_MAX_DEFS      128
#define KEX_MAX_FLAGS     256
#define KEX_PATH_MAX      1024

typedef struct {
    char path[KEX_PATH_MAX];
    int lang;
} kex_source_t;

typedef struct {
    char sources[KEX_MAX_SOURCES][KEX_PATH_MAX];
    int source_langs[KEX_MAX_SOURCES];
    int source_count;

    char output[KEX_PATH_MAX];
    int output_format;
    char name[64];
    int is_shared;

    int opt_level;
    bool debug_info;
    bool strip;
    bool pie;
    bool static_link;

    char inc_dirs[KEX_MAX_INC_DIRS][KEX_PATH_MAX];
    int inc_dir_count;
    char lib_dirs[KEX_MAX_LIB_DIRS][KEX_PATH_MAX];
    int lib_dir_count;
    char defines[KEX_MAX_DEFS][256];
    int define_count;
    char extra_cflags[KEX_MAX_FLAGS][256];
    int extra_cflag_count;
    char extra_ldflags[KEX_MAX_FLAGS][256];
    int extra_ldflag_count;
    char libs[KEX_MAX_FLAGS][256];
    int lib_count;

    char cc[KEX_PATH_MAX];
    char cxx[KEX_PATH_MAX];
    char ld[KEX_PATH_MAX];
    char ar[KEX_PATH_MAX];
    char objcopy[KEX_PATH_MAX];

    bool verbose;
    bool keep_temps;
    bool has_graphics;
} kex_config_t;

typedef struct {
    char obj_path[KEX_PATH_MAX];
    int lang;
    bool compiled;
} kex_obj_t;

typedef struct {
    kex_obj_t objs[KEX_MAX_SOURCES];
    int obj_count;
    char merged_obj[KEX_PATH_MAX];
    char output[KEX_PATH_MAX];
    bool success;
} kex_build_result_t;

#endif