demo: demo.c pg.lib
	cl -nologo -Zi -Ox -fp:fast demo.c pg.lib -link /incremental:no
pg.lib: pg.c pg.h pgOpenType.c
	cl -nologo -c -Zi -Zl -Ox -fp:fast pg.c pgOpenType.c platform.c
	lib -nologo pg.obj pgOpenType.obj platform.obj