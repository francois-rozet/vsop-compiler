# Macros
ALL = vsopc

SRCDIR = src/
BINDIR = bin/
EXT = yy.c

CXX = g++
CXXFLAGS = -std=c++14 -O3

# Executable files
all: $(ALL)

$(ALL): %: $(BINDIR)%.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BINDIR)%.o: $(SRCDIR)%.$(EXT)
	mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(SRCDIR)%.$(EXT): $(SRCDIR)%.lex
	flex -o $@ $<

# PHONY
.PHONY: clean dist-clean install-tools

clean:
	rm -rf $(BINDIR) $(wildcard $(SRCDIR)*.$(EXT))

dist-clean: clean
	rm -rf $(ALL)

install-tools:
	sudo apt install flex
