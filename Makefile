lazfs : lazfs.o log.o util.o cache.o compress_laz.o
	gcc -g -o lazfs lazfs.o util.o log.o cache.o compress_laz.o `pkg-config fuse --libs` -llas_c

lazfs.o : lazfs.c log.h params.h
	gcc -g -Wall `pkg-config fuse --cflags` -c lazfs.c

util.o: util.c util.h
	gcc -g -Wall `pkg-config fuse --cflags` -c util.c

log.o : log.c log.h params.h
	gcc -g -Wall `pkg-config fuse --cflags` -c log.c

cache.o: cache.c cache.h
	gcc -g -Wall -c cache.c

compress_laz.o: compress_laz.h compress_laz.c
	gcc -g -Wall `pkg-config fuse --cflags` -c compress_laz.c

clean:
	rm -f lazfs *.o

dist:
	rm -rf fuse-tutorial/
	mkdir fuse-tutorial/
	cp ../*.html fuse-tutorial/
	mkdir fuse-tutorial/example/
	mkdir fuse-tutorial/example/mountdir/
	mkdir fuse-tutorial/example/rootdir/
	echo "a bogus file" > fuse-tutorial/example/rootdir/bogus.txt
	mkdir fuse-tutorial/src
	cp Makefile COPYING COPYING.LIB *.c *.h fuse-tutorial/src/
	tar cvzf ../../fuse-tutorial.tgz fuse-tutorial/
	rm -rf fuse-tutorial/
