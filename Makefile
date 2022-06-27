CFLAGS=-std=c11 -pipe -Wall -Wextra -Wpedantic -Wvla -Wshadow -Wstrict-aliasing -fstrict-aliasing -O0 $$(pkg-config --cflags sdl2)
LDLIBS=$$(pkg-config --libs sdl2)

all:line-codes

line-codes:main.c nuklear.h nuklear_sdl_renderer.h
	$(CC) $(CFLAGS) -o line-codes main.c $(LDLIBS)

clean:
	rm -f line-codes
