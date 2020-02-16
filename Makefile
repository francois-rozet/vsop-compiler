# Macros
ALL = vsopc

SRCDIR = src/
BINDIR = bin/
EXT = cpp

CXX = g++
CXXFLAGS = -std=c++14 -O3 -Wall -Wextra

# Source Files
SRCS = $(wildcard $(SRCDIR)*.$(EXT))
OBJS = $(patsubst $(SRCDIR)%.$(EXT), $(BINDIR)%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)
XOBJS = $(filter-out $(patsubst %, $(BINDIR)%.o, $(ALL)), $(OBJS))

# Executable files
all: $(ALL)

$(ALL): %: $(BINDIR)%.o $(XOBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Dependency files
$(BINDIR)%.d: $(SRCDIR)%.$(EXT)
	mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $< -MM -MT $(patsubst $(SRCDIR)%.$(EXT), $(BINDIR)%.o, $<) -MF $@

# Object files
-include $(DEPS)

$(BINDIR)%.o: $(SRCDIR)%.$(EXT)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Phony
.PHONY: clean dist-clean install-tools

clean:
	rm -rf $(BINDIR)

dist-clean: clean
	rm -rf $(ALL)

install-tools:
	echo tools installed
