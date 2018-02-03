CFLAGS         += -Ideps -I. -Wall -pedantic -g
DIRDEPS         = deps
SRC             = $(wildcard ./*.c)
DEPS            = $(wildcard $(DIRDEPS)/*/*.c)
OBJS            = $(DEPS:.c=.o) $(SRC:.c=.o)
BOOLTAB         = booltab
TESTSCRIPT      = ./runtests.sh

default: $(BOOLTAB)


$(BOOLTAB): $(OBJS)
	$(CC) -o $@ $(OBJS)
	chmod u+x $@

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

test: $(BOOLTAB) $(TESTSCRIPT)
	@$(TESTSCRIPT)

clean:
	$(foreach c, $(OBJS), rm -f $(c))
	rm -f $(BOOLTAB)
