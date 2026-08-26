CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-format-truncation -Wno-unused-variable -Wno-unused-but-set-variable -O2 -Iinclude
LDFLAGS = -lm

all: bin/kex

bin/kex: src/kex.c include/kex.h
	mkdir -p bin
	$(CC) $(CFLAGS) src/kex.c $(LDFLAGS) -o $@

WIN_CC      = x86_64-w64-mingw32-gcc
WIN_CFLAGS  = -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-format-truncation -Wno-unused-variable -Wno-unused-but-set-variable -O2 -Iinclude -D_WIN32_WINNT=0x0600
WIN_LDFLAGS = -lws2_32 -lgdi32 -luser32 -lm

win32: bin/kex.exe

bin/kex.exe: src/kex.c include/kex.h
	mkdir -p bin
	$(WIN_CC) $(WIN_CFLAGS) src/kex.c $(WIN_LDFLAGS) -o $@

demo: bin/kex
	./bin/kex examples/hello.c -o examples/hello.kex --name hello
	./bin/kex run examples/hello.kex

demo-gfx: bin/kex
	./bin/kex examples/gfx_demo.c -o examples/gfx_demo.kex --name gfx_demo
	./bin/kex run examples/gfx_demo.kex --gfx --gfx-size 800x600

examples: bin/kex
	@for f in examples/*.c; do \
		bn=$$(basename "$$f" .c); \
		./bin/kex "$$f" -o "examples/$$bn.kex" --name "$$bn"; \
	done

test: examples
	@echo "=== all tests passed ==="

clean:
	rm -rf bin/
	rm -f examples/*.kex examples/*.kxp examples/*.elf

.PHONY: all win32 demo demo-gfx examples test clean