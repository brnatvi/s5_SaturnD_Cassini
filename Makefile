all: cassini saturnd

cassini: src/cassini.c src/client-request.c src/timing-text-io.c
	gcc -I include -o cassini src/cassini.c src/client-request.c src/timing-text-io.c src/run-daemon.c

saturnd: src/saturnd.c src/listd.c
	gcc -I include -o saturnd src/saturnd.c src/client-request.c src/listd.c src/run-daemon.c src/timing-text-io.c

distclean:
	rm -rf cassini saturnd *.o

clean:
	rm -rf cassini saturnd *.o	