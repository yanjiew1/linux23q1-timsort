CC = gcc
CFLAGS = -O2

all: main

OBJS := main.o list_sort.o shiverssort.o \
        timsort.o

deps := $(OBJS:%.o=.%.o.d)

main: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<

test: main
	@./main

clean:
	rm -f $(OBJS) $(deps) *~ main
	rm -rf *.dSYM

-include $(deps)
