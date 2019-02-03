pg.lib: pg.c pg.h pgOpenType.c pgSimpleFont.c pw.c pw.h pwPlatformWinGdi.c platform.c
	cl -nologo -c -WX -Zi -Zl -Ox -fp:fast -I . -we4013 -wd4005 pg.c pgOpenType.c pgSimpleFont.c platform.c pw.c pwPlatformWinGdi.c
	lib -nologo *.obj
clean:
	del *.exe *.obj *.lib *.pdb