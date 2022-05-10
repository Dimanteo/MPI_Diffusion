CC = mpic++
CFLAGS = -Wall -g

net: main.cpp net.cpp
	mpic++ $(CFLAGS) -o $@ $^

run: net
	mpirun -n 4 ./net config

.PHONY: clean
clean:
	rm -f *.d *.o *.so *.a net

# -include *.d