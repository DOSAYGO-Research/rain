CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3
DEPFLAGS = -MMD -MF $(@:.o=.d)

OBJDIR = rain/obj
BUILDDIR = rain/bin
SRCS = $(wildcard src/*.cpp)
OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

all: directories rainsum link

directories: ${OBJDIR} ${BUILDDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$@ $^

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

link:
	ln -sf rain/bin/rainsum

.PHONY: install

install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum

