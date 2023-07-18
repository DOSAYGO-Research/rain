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
STORM_WASM_TARGET = wasm/rainstorm.js
BOW_WASM_SOURCE = src/rainbow.cpp
BOW_WASM_TARGET = wasm/rainbow.js
# we need to add stringToUTF8 to exported functions rather than runtime methods because of: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#3135---040323
STORM_EMCCFLAGS = -O3 -s WASM=1 -s EXPORTED_FUNCTIONS="['_rainstormHash64', '_rainstormHash128', '_rainstormHash256', '_rainstormHash512', 'stringToUTF8', 'lengthBytesUTF8', '_malloc', '_free']" -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']" -s WASM_BIGINT=1 -s ALLOW_MEMORY_GROWTH=1
BOW_EMCCFLAGS = -O3 -s WASM=1 -s EXPORTED_FUNCTIONS="['_rainbowHash64', '_rainbowHash128', '_rainbowHash256', 'stringToUTF8', 'lengthBytesUTF8', '_malloc', '_free']" -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']" -s WASM_BIGINT=1 -s ALLOW_MEMORY_GROWTH=1

all: directories node_modules rainsum link rainwasm wasmhtml

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

rainstorm: $(STORM_WASM_TARGET)

rainbow: $(BOW_WASM_TARGET)

wasmhtml: $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE)
	@[ -d web ] || mkdir -p web
	emcc $(STORM_EMCCFLAGS) $(BOW_EMCCFLAGS) -o web/rain.html $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE)

$(STORM_WASM_TARGET): $(STORM_WASM_SOURCE)
	@[ -d wasm ] || mkdir -p wasm
	emcc $(STORM_EMCCFLAGS) -s MODULARIZE=1 -s 'EXPORT_NAME="createRainstormModule"' -o $(STORM_WASM_TARGET) $(STORM_WASM_SOURCE)

$(BOW_WASM_TARGET): $(BOW_WASM_SOURCE)
	@[ -d wasm ] || mkdir -p wasm
	emcc $(BOW_EMCCFLAGS) -s MODULARIZE=1 -s 'EXPORT_NAME="createRainbowModule"' -o $(BOW_WASM_TARGET) $(BOW_WASM_SOURCE)

rainwasm: $(STORM_WASM_TARGET) $(BOW_WASM_TARGET)

link:
	ln -sf rain/bin/rainsum

.PHONY: install

install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum $(WASMDIR) js/node_modules scripts/node_modules


