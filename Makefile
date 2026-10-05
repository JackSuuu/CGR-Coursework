# Build for CGR26_renderer.  Standard library only: no external dependencies.
#
#   make            build output/CGR26_renderer
#   make clean      remove objects and the executable
#   make smoke      build, then render every scene in scenes/
#
# Override the compiler or the standard with, e.g.:
#   make CXX=g++ CXXFLAGS="-O2 -std=c++20 -Wall"

SRCDIR    := source
OBJDIR    := build

CXX      ?= c++
STD      ?= -std=c++17
OPT      ?= -O3
WARN     ?= -Wall -Wextra
# All includes are relative to source/, so that source files can use paths like
# "core/vec.h" and "scene/shape.h" without any external include directories.
INC      := -I$(SRCDIR)
CXXFLAGS ?= $(STD) $(OPT) $(WARN) $(INC) -fno-strict-aliasing
LDFLAGS  ?=
LDLIBS   ?= -lm

OUT       := output/CGR26_renderer

SRCS := $(shell find $(SRCDIR) -name '*.cpp' | sort)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean smoke scenes

all: $(OUT)

$(OUT): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "built $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

scenes: $(OUT)
	@./renderReportedImages.sh

smoke: scenes

clean:
	rm -rf $(OBJDIR) $(OUT)
