cassini: src/cassini.c src/client-request.c src/timing-text-io.c
	gcc -I include -o cassini src/cassini.c src/client-request.c src/timing-text-io.c 

clean:
	rm -rf cassini *.o