CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2
DEPFLAGS = -MMD -MF $(@:.o=.d)

OBJDIR = obj
BUILDDIR = bin
SRCS = rainsum.cpp
OBJS = $(addprefix $(OBJDIR)/,$(SRCS:.cpp=.o))
DEPS = $(OBJS:.o=.d)

all: directories rainsum

directories: ${OBJDIR} ${BUILDDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$@ $^

$(OBJDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum

