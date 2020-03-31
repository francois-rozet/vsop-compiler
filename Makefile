# Macros
ALL = vsopc

SRCDIR = src/
BINDIR = bin/

CXX = g++
CXXFLAGS = -std=c++14 -O3

# Executable files
all: $(ALL)

$(ALL): %c: $(SRCDIR)%c.cpp $(SRCDIR)%.yy.c $(SRCDIR)%.tab.c
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SRCDIR)%.tab.c: $(SRCDIR)%.y
	bison -o $@ -d $^

$(SRCDIR)%.yy.c: $(SRCDIR)%.lex
	flex -o $@ $^

# PHONY
.PHONY: clean dist-clean install-tools

clean:
	rm -rf $(BINDIR) $(wildcard $(SRCDIR)*.c) $(wildcard $(SRCDIR)*.h)

dist-clean: clean
	rm -rf $(ALL)

install-tools:
	sudo apt install flex bison
