all: cassini saturnd

cassini: src/cassini.c src/client-request.c src/timing-text-io.c
	gcc -I include -o cassini src/cassini.c src/client-request.c src/timing-text-io.c 

saturnd: src/saturnd.c src/listd.c
	gcc -I include -o saturnd src/saturnd.c src/listd.c

distclean:
	rm -rf cassini saturnd *.o

clean:
	rm -rf cassini saturnd *.o	