# KexKit Makefile (Linux / WSL + Windows 交叉编译)
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Iinclude
AR      = ar

LIB_SRC = src/kex_crc32.c src/kex_api_table.c src/kex_loader.c
LIB_OBJ = $(LIB_SRC:.c=.o)

all: libkex.a kexc kex_interp kex

libkex.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kexc: src/kexc.c libkex.a
	$(CC) $(CFLAGS) -DKEX_STANDALONE src/kexc.c -L. -lkex -o $@

kex_interp: tools/kex_interp.c libkex.a
	$(CC) $(CFLAGS) -DKEX_STANDALONE tools/kex_interp.c -L. -lkex -o $@

# 统一程序: 编译器+解释器合体
kex: src/kex_main.c src/kexc.c tools/kex_interp.c libkex.a
	$(CC) $(CFLAGS) src/kex_main.c src/kexc.c tools/kex_interp.c -L. -lkex -o $@

# ===== Windows 交叉编译 (在 WSL/Linux 中执行) =====
WIN_CC      = x86_64-w64-mingw32-gcc
WIN_AR      = x86_64-w64-mingw32-ar
WIN_CFLAGS  = -Wall -Wextra -O2 -Iinclude -D_WIN32_WINNT=0x0600
WIN_LDFLAGS = -lws2_32

WIN_LIB_OBJ = $(LIB_SRC:.c=.win.o)

win32: kexc.exe kex_interp.exe kex.exe

libkex_win.a: $(WIN_LIB_OBJ)
	$(WIN_AR) rcs $@ $^

src/%.win.o: src/%.c
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

kexc.exe: src/kexc.c libkex_win.a
	$(WIN_CC) $(WIN_CFLAGS) -DKEX_STANDALONE src/kexc.c -L. -lkex_win -o $@

kex_interp.exe: tools/kex_interp.c libkex_win.a
	$(WIN_CC) $(WIN_CFLAGS) -DKEX_STANDALONE tools/kex_interp.c -L. -lkex_win $(WIN_LDFLAGS) -o $@

# Windows 统一程序
kex.exe: src/kex_main.c src/kexc.c tools/kex_interp.c libkex_win.a
	$(WIN_CC) $(WIN_CFLAGS) src/kex_main.c src/kexc.c tools/kex_interp.c -L. -lkex_win $(WIN_LDFLAGS) -o $@

# 示例: 编译并运行 hello.kex
demo: kexc kex_interp
	./kexc examples/hello.c examples/hello.kex --name hello
	./kex_interp examples/hello.kex

# 示例: 编译并运行 .kxp 动态库测试
demo-kxp: kexc kex_interp
	./kexc examples/libhello.c examples/libhello.kxp --name libhello --kxp
	./kexc examples/test_kxp.c examples/test_kxp.kex --name test_kxp
	./kex_interp -Lexamples examples/test_kxp.kex

# 示例: 文件 I/O
demo-fileio: kexc kex_interp
	./kexc examples/fileio.c examples/fileio.kex --name fileio
	./kex_interp examples/fileio.kex

# 示例: 便利工具函数
demo-utils: kexc kex_interp
	./kexc examples/utils.c examples/utils.kex --name utils
	./kex_interp examples/utils.kex

# 编译所有示例 (不运行)
examples: kexc
	./kexc examples/hello.c examples/hello.kex --name hello
	./kexc examples/libhello.c examples/libhello.kxp --name libhello --kxp
	./kexc examples/test_kxp.c examples/test_kxp.kex --name test_kxp
	./kexc examples/fileio.c examples/fileio.kex --name fileio
	./kexc examples/utils.c examples/utils.kex --name utils

# 运行所有测试
test: examples kex_interp
	@echo "=== 测试 hello ==="
	./kex_interp examples/hello.kex
	@echo "=== 测试 .kxp 动态库 ==="
	./kex_interp -Lexamples examples/test_kxp.kex
	@echo "=== 测试文件 I/O ==="
	./kex_interp examples/fileio.kex
	@echo "=== 测试工具函数 ==="
	./kex_interp examples/utils.kex
	@echo "=== 所有测试通过 ==="

clean:
	rm -f src/*.o src/*.win.o libkex.a libkex_win.a kexc kex_interp kex kexc.exe kex_interp.exe kex.exe examples/*.kex examples/*.kxp examples/*.kexc.o

.PHONY: all win32 demo demo-kxp demo-fileio demo-utils examples test clean
