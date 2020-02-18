CFLAGS=-std=gnu99 -O3 -Wall -Wextra -Werror -Wstrict-prototypes -Wno-parentheses# -Wno-unused-function

demo: demo.c libpg2.a
	$(CC) $(CFLAGS) -I. -L. -odemo demo.c -lpg2 -lSDL2 -lm
	./demo

libpg2.a: pg.c pgOpenType.c platform.linux.c pg.h
	$(CC) $(CFLAGS) -c pg.c pgOpenType.c platform.linux.c
	ar rcs libpg2.a *.o

clean:
	rm *.o *.a
