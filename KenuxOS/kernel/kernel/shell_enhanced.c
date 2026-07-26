

#include <arch/types.h>
#include <arch/fs.h>
#include <arch/memory.h>
#include <arch/vga.h>
#include <string.h>
#include <stdio.h>
#include <kapi.h>
#include <slab.h>

static int isalnum_c(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

extern int shell_execute(const char* cmd, int argc, char* argv[]);
extern int kapi_shell_read_line(char* buf, uint64_t size);

#define SHELL_MAX_ARGS      32
#define SHELL_MAX_PIPES     8
#define SHELL_MAX_ENVS      64
#define SHELL_ENV_NAME_MAX  64
#define SHELL_ENV_VAL_MAX   256
#define SHELL_HISTORY_MAX   16

typedef struct {
    char name[SHELL_ENV_NAME_MAX];
    char value[SHELL_ENV_VAL_MAX];
} env_var_t;

static env_var_t env_vars[SHELL_MAX_ENVS];
static uint32_t env_count = 0;

static char* history[SHELL_HISTORY_MAX];
static uint32_t history_count = 0;
static uint32_t history_index = 0;

static const char* shell_getenv(const char* name)
{
    for (uint32_t i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

static void shell_setenv(const char* name, const char* value)
{
    for (uint32_t i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            strncpy(env_vars[i].value, value, SHELL_ENV_VAL_MAX - 1);
            env_vars[i].value[SHELL_ENV_VAL_MAX - 1] = '\0';
            return;
        }
    }
    if (env_count < SHELL_MAX_ENVS) {
        strncpy(env_vars[env_count].name, name, SHELL_ENV_NAME_MAX - 1);
        env_vars[env_count].name[SHELL_ENV_NAME_MAX - 1] = '\0';
        strncpy(env_vars[env_count].value, value, SHELL_ENV_VAL_MAX - 1);
        env_vars[env_count].value[SHELL_ENV_VAL_MAX - 1] = '\0';
        env_count++;
    }
}

static void shell_expand_vars(char* dst, const char* src, uint64_t max)
{
    uint64_t di = 0;
    while (*src && di < max - 1) {
        if (*src == '$') {
            src++;
            char name[SHELL_ENV_NAME_MAX];
            uint32_t ni = 0;
            while (*src && (isalnum_c(*src) || *src == '_') && ni < SHELL_ENV_NAME_MAX - 1) {
                name[ni++] = *src++;
            }
            name[ni] = '\0';
            const char* val = shell_getenv(name);
            if (val) {
                while (*val && di < max - 1) {
                    dst[di++] = *val++;
                }
            }
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

static void shell_unquote(char* dst, const char* src, uint64_t max)
{
    uint64_t di = 0;
    char quote = 0;
    while (*src && di < max - 1) {
        if (!quote && (*src == '"' || *src == '\'')) {
            quote = *src;
            src++;
            continue;
        }
        if (quote && *src == quote) {
            quote = 0;
            src++;
            continue;
        }
        dst[di++] = *src++;
    }
    dst[di] = '\0';
}

typedef struct {
    char* argv[SHELL_MAX_ARGS];
    int argc;
    char* input_file;
    char* output_file;
    int append_output;
    int background;
} shell_cmd_t;

static int shell_parse_line(const char* line, shell_cmd_t* cmds, int max_cmds)
{
    int cmd_count = 0;
    char* tokens[128];
    int tok_count = 0;

    char expanded[512];
    shell_expand_vars(expanded, line, sizeof(expanded));

    char* p = expanded;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char token[256];
        uint32_t ti = 0;
        char quote = 0;

        while (*p && (quote || (*p != ' ' && *p != '\t'))) {
            if (*p == '"' || *p == '\'') {
                if (!quote) quote = *p;
                else if (quote == *p) quote = 0;
                else token[ti++] = *p;
            } else {
                token[ti++] = *p;
            }
            p++;
            if (ti >= 255) break;
        }
        token[ti] = '\0';

        if (ti > 0) {
            tokens[tok_count] = (char*)kmalloc(ti + 1);
            strcpy(tokens[tok_count], token);
            tok_count++;
        }
    }

    memset(cmds, 0, sizeof(shell_cmd_t) * max_cmds);
    int curr = 0;
    int arg = 0;

    for (int i = 0; i < tok_count; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            cmds[curr].argc = arg;
            curr++;
            arg = 0;
            if (curr >= max_cmds) break;
        } else if (strcmp(tokens[i], ">") == 0 && i + 1 < tok_count) {
            cmds[curr].output_file = tokens[++i];
            cmds[curr].append_output = 0;
        } else if (strcmp(tokens[i], ">>") == 0 && i + 1 < tok_count) {
            cmds[curr].output_file = tokens[++i];
            cmds[curr].append_output = 1;
        } else if (strcmp(tokens[i], "<") == 0 && i + 1 < tok_count) {
            cmds[curr].input_file = tokens[++i];
        } else if (strcmp(tokens[i], "&") == 0) {
            cmds[curr].background = 1;
        } else {
            if (arg < SHELL_MAX_ARGS - 1) {
                cmds[curr].argv[arg++] = tokens[i];
            }
        }
    }
    cmds[curr].argc = arg;
    cmd_count = curr + 1;

    return cmd_count;
}

static int shell_create_pipe(int pipefd[2])
{

    static int pipe_counter = 0;
    char path[64];
    snprintf(path, sizeof(path), "/tmp/pipe_%d", pipe_counter++);

    vfs_node_t* node = vfs_create_node(path, FS_TYPE_FIFO);
    if (!node) return -1;

    (void)node;
    pipefd[0] = -1;
    pipefd[1] = -1;
    return 0;
}

static int shell_exec_pipeline(shell_cmd_t* cmds, int cmd_count)
{
    if (cmd_count == 0) return -1;
    if (cmd_count == 1) {

        if (cmds[0].argc == 0) return 0;

        if (strcmp(cmds[0].argv[0], "export") == 0 && cmds[0].argc >= 2) {
            char* eq = strchr(cmds[0].argv[1], '=');
            if (eq) {
                *eq = '\0';
                shell_setenv(cmds[0].argv[1], eq + 1);
            }
            return 0;
        }
        if (strcmp(cmds[0].argv[0], "env") == 0) {
            for (uint32_t i = 0; i < env_count; i++) {
                char vbuf[320];
                snprintf(vbuf, sizeof(vbuf), "%s=%s\n", env_vars[i].name, env_vars[i].value);
                vga_print(vbuf);
            }
            return 0;
        }

        return shell_execute(cmds[0].argv[0], cmds[0].argc, cmds[0].argv);
    }

    for (int i = 0; i < cmd_count; i++) {
        shell_execute(cmds[i].argv[0], cmds[i].argc, cmds[i].argv);
    }
    return 0;
}

void shell_enhanced_run(void);

int shell_execute(const char* cmd, int argc, char* argv[])
{
    (void)cmd; (void)argc; (void)argv;
    return 0;
}

int kapi_shell_read_line(char* buf, uint64_t size)
{
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    return 0;
}

void shell_init(void)
{
}

void shell_run(void)
{
    shell_enhanced_run();
}

void shell_enhanced_run(void)
{

    shell_setenv("PATH", "/bin:/usr/bin");
    shell_setenv("HOME", "/root");
    shell_setenv("USER", "root");
    shell_setenv("SHELL", "/bin/sh");
    shell_setenv("TERM", "linux");

    char line[512];
    shell_cmd_t cmds[SHELL_MAX_PIPES];

    for (;;) {
        vga_puts("$ ");
        if (!kapi_shell_read_line(line, sizeof(line))) break;

        if (history_count < SHELL_HISTORY_MAX) {
            history[history_count] = (char*)kmalloc(strlen(line) + 1);
            if (history[history_count]) {
                strcpy(history[history_count], line);
                history_count++;
            }
        }

        int cmd_count = shell_parse_line(line, cmds, SHELL_MAX_PIPES);
        if (cmd_count > 0) {
            shell_exec_pipeline(cmds, cmd_count);
        }

        for (int i = 0; i < cmd_count; i++) {
            for (int j = 0; j < cmds[i].argc; j++) {
                if (cmds[i].argv[j]) kfree(cmds[i].argv[j]);
            }
        }
    }
}
