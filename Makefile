GCC = gcc
GDB = -g
C99 = -std=c99
PTHREAD = -pthread

httpserver: main.o do_http.o
		$(GCC) $(GDB) $^ $(PTHREAD) -o $@
main.o : httpserver.c httpserver.h do_http.h
		$(GCC) $(GDB) -c $< $(PTHREAD) -o $@ $(C99)
do_http.o : do_http.c do_http.h list.h
		$(GCC) $(GDB) -c $<  -o $@ $(C99)
clean:
		rm -rf httpserver
