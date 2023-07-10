CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

OBJDIR = obj
BUILDDIR = bin
SRCS = rainsum.cpp
OBJS = $(addprefix $(OBJDIR)/,$(SRCS:.cpp=.o))

all: directories rainsum

directories: ${OBJDIR} ${BUILDDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$@ $^

$(OBJDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum

