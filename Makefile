demo: demo.c pg.lib
	cl -nologo -Zi -Ox -fp:fast demo.c pg.lib
pg.lib: pg.c pg.h
	cl -nologo -c -Zi -Zl -Ox -fp:fast pg.c
	lib -nologo pg.obj