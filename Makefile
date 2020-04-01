# Macros
ALL = vsopc

SRCDIR = src/
BINDIR = bin/
EXT = cpp

CXX = g++
CXXFLAGS = -std=c++14 -O3

# Source files
SRCS = $(wildcard $(SRCDIR)*.$(EXT))
OBJS = $(patsubst $(SRCDIR)%.$(EXT), $(BINDIR)%.o, $(SRCS))

# Executable files
all: $(ALL)

$(ALL): %c: $(SRCDIR)%.yy.c $(SRCDIR)%.tab.c $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SRCDIR)%.tab.c: $(SRCDIR)%.y
	bison -o $@ -d $^

$(SRCDIR)%.yy.c: $(SRCDIR)%.lex
	flex -o $@ $^

$(BINDIR)%.o: $(SRCDIR)%.$(EXT)
	mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# PHONY
.PHONY: clean dist-clean install-tools

clean:
	rm -rf $(BINDIR) $(wildcard $(SRCDIR)*.c) $(wildcard $(SRCDIR)*.h)

dist-clean: clean
	rm -rf $(ALL)

install-tools:
	sudo apt install flex bison
