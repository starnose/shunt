

CLANG   = /usr/bin/clang
GCC     = /usr/bin/gcc
SOURCES = $(wildcard ./*.c)

LIBRARIES = pthread crypto
LIB_LINK  =  $(patsubst %, -l%,$(LIBRARIES))

DEBUGFLAGS = -g -DDEBUG_MODE
CFLAGS    = -Wall -Wextra $(DEBUGFLAGS)
EXTRA_INCLUDES = -I/sw/include/

default: all

shuntgcc: $(SOURCES)
	$(GCC) $(CFLAGS) -o shuntgcc $(SOURCES) $(EXTRA_INCLUDES) $(LIB_LINK)

shuntclang: $(SOURCES)
	$(CLANG) $(CFLAGS) -o shuntclang $(SOURCES) $(EXTRA_INCLUDES) $(LIB_LINK)

shunt: shuntclang
	@cp shuntclang shunt

all: shuntclang

clean:
	rm -f shuntclang shuntgcc shunt

version:
	$(CLANG) --version
	$(GCC) --version
