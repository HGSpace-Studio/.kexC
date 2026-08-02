# KexKit Makefile (单文件版)
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Iinclude

all: kex

# 单文件编译: 所有代码已合并到 src/kex.c
kex: src/kex.c
	$(CC) $(CFLAGS) src/kex.c -o $@

# ===== Windows 交叉编译 (在 WSL/Linux 中执行) =====
WIN_CC      = x86_64-w64-mingw32-gcc
WIN_CFLAGS  = -Wall -Wextra -O2 -Iinclude -D_WIN32_WINNT=0x0600
WIN_LDFLAGS = -lws2_32

win32: kex.exe

kex.exe: src/kex.c
	$(WIN_CC) $(WIN_CFLAGS) src/kex.c $(WIN_LDFLAGS) -o $@

# 示例: 编译并运行 hello.kex
demo: kex
	./kex compile examples/hello.c examples/hello.kex --name hello
	./kex run examples/hello.kex

# 示例: 编译并运行 .kxp 动态库测试
demo-kxp: kex
	./kex compile examples/libhello.c examples/libhello.kxp --name libhello --kxp
	./kex compile examples/test_kxp.c examples/test_kxp.kex --name test_kxp
	./kex run -Lexamples examples/test_kxp.kex

# 示例: 文件 I/O
demo-fileio: kex
	./kex compile examples/fileio.c examples/fileio.kex --name fileio
	./kex run examples/fileio.kex

# 示例: 便利工具函数
demo-utils: kex
	./kex compile examples/utils.c examples/utils.kex --name utils
	./kex run examples/utils.kex

# 编译所有示例 (不运行)
examples: kex
	./kex compile examples/hello.c examples/hello.kex --name hello
	./kex compile examples/libhello.c examples/libhello.kxp --name libhello --kxp
	./kex compile examples/test_kxp.c examples/test_kxp.kex --name test_kxp
	./kex compile examples/fileio.c examples/fileio.kex --name fileio
	./kex compile examples/utils.c examples/utils.kex --name utils

# 运行所有测试
test: examples
	@echo "=== 测试 hello ==="
	./kex run examples/hello.kex
	@echo "=== 测试 .kxp 动态库 ==="
	./kex run -Lexamples examples/test_kxp.kex
	@echo "=== 测试文件 I/O ==="
	./kex run examples/fileio.kex
	@echo "=== 测试工具函数 ==="
	./kex run examples/utils.kex
	@echo "=== 所有测试通过 ==="

clean:
	rm -f kex kex.exe examples/*.kex examples/*.kxp examples/*.kexc.o

.PHONY: all win32 demo demo-kxp demo-fileio demo-utils examples test clean
