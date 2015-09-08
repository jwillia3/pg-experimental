demo: demo.c pg.lib
	cl -nologo -Zi -Ox -fp:fast demo.c pg.lib -link /incremental:no
pg.lib: pg.c pg.h pgOpenType.c pw.c pw.h pwPlatformWinGdi.c platform.c
	cl -nologo -c -Zi -Zl -Ox -fp:fast pg.c pgOpenType.c platform.c pw.c pwPlatformWinGdi.c
	del demo.obj
	lib -nologo *.obj
clean:
	del demo.exe *.obj *.lib *.pdb