# lnk6x: built like link, masm and asm6x and the compilers they serve - C++14, -Wall -Wextra
# -Werror -pedantic, objects outside the checkout, the program where BINDIR says. RIDE's
# workspace.mk calls this with BINDIR=bin and OBJDIR=bin/obj/lnk6x so that lnk6x.exe lands
# beside the editor, where settings.json names it as the linker for the C6747 in place of
# TI's.
ifeq ($(origin CXX),default)
  ifneq ($(shell command -v clang++ 2>/dev/null),)
    CXX := clang++
  else
    CXX := g++
  endif
endif
CXXFLAGS = -std=c++14 -O2 -g -Wall -Wextra -Werror -pedantic
SRCS     = $(filter src/%.cpp,$(wildcard src/*.cpp))
OBJDIR  ?= ../build/LNK6x/obj
OBJS     = $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRCS))
BINDIR  ?= build
TARGET   = $(BINDIR)/lnk6x.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

# Both beds run, whatever the first one says, and the target fails if either did.
test: $(TARGET)
	@LNK=$(TARGET) sh tests/run.sh; r=$$?; echo; \
	 LNK=$(TARGET) sh tests/bad.sh; b=$$?; \
	 [ $$r -eq 0 ] && [ $$b -eq 0 ]

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all test clean
