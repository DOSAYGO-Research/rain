CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3 
DEPFLAGS = -MMD -MF $(@:.o=.d)
LDFLAGS = 

OBJDIR = rain/obj
BUILDDIR = rain/bin
SRCS = $(wildcard src/*.cpp)
OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

WASMDIR = wasm
STORM_WASM_SOURCE = src/rainstorm.cpp
BOW_WASM_SOURCE = src/rainbow.cpp
# we need to add stringToUTF8 to exported functions rather than runtime methods because of: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#3135---040323
EMCCFLAGS = -O3 -s WASM=1 -s EXPORTED_FUNCTIONS="['_rainstormHash64', '_rainstormHash128', '_rainstormHash256', '_rainstormHash512', 'stringToUTF8', 'lengthBytesUTF8', '_malloc', '_free']" -s EXPORTED_FUNCTIONS="['_rainbowHash64', '_rainbowHash128', '_rainbowHash256', 'stringToUTF8', 'lengthBytesUTF8', '_malloc', '_free']" -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']" -s WASM_BIGINT=1 -s ALLOW_MEMORY_GROWTH=1

all: directories node_modules rainsum link rainwasm

directories: ${OBJDIR} ${BUILDDIR} ${WASMDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

node_modules:
	(test ! -d ./js/node_modules && cd js && npm i && cd ..) || :
	(test ! -d ./scripts/node_modules && cd scripts && npm i && cd ..) || :

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

${WASMDIR}:
	mkdir -p ${WASMDIR}

rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BUILDDIR)/$@ $^

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

rainwasm: $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE)
	@[ -d docs ] || mkdir -p docs
	@[ -d wasm ] || mkdir -p wasm
	# -s MODULARIZE=1 -s 'EXPORT_NAME="createRainstormModule"' 
	emcc $(EMCCFLAGS) -o docs/rain.html $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE)
	cp {docs/rain.wasm,docs/rain.js} wasm/
	rm docs/rain.html

link:
	ln -sf rain/bin/rainsum

.PHONY: install

install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum $(WASMDIR) js/node_modules scripts/node_modules


