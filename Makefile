cassini: src/cassini.c
	gcc -I include -o cassini src/cassini.c

clean:
	rm -rf cassini *.o