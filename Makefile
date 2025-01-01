CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O3 
CXXFLAGS += -g -fsanitize=thread
CXXFLAGS += -isysroot $(shell xcrun --show-sdk-path)
CXXFLAGS += -fuse-ld=ld -lz
DEPFLAGS = -MMD -MF $(@:.o=.d)
LDFLAGS = 

EMCCFLAGS = -O3 -s WASM=1 -s EXPORTED_FUNCTIONS="['_rainbowHash64', '_rainbowHash128', '_rainbowHash256', 'stringToUTF8', 'lengthBytesUTF8', '_malloc', '_free']" -s EXPORTED_RUNTIME_METHODS="['wasmExports', 'ccall', 'cwrap']" -s WASM_BIGINT=1 -s ALLOW_MEMORY_GROWTH=1 -g 

OBJDIR = rain/obj
BUILDDIR = rain/bin
SRCS = $(wildcard src/*.cpp)
OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

WASMDIR = js/wasm
STORM_WASM_SOURCE = src/rainstorm.cpp
BOW_WASM_SOURCE = src/rainbow.cpp
WASM_OUTPUT = docs/rain.wasm
JS_OUTPUT = docs/rain.cjs

all: directories node_modules rainsum link rainwasm

directories: ${OBJDIR} ${BUILDDIR} ${WASMDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

node_modules:
	@(test ! -d ./js/node_modules && cd js && npm i && cd ..) || :
	@(test ! -d ./scripts/node_modules && cd scripts && npm i && cd ..) || :

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

${WASMDIR}:
	mkdir -p ${WASMDIR}

rainsum: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BUILDDIR)/$@ $^

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

rainwasm: $(WASM_OUTPUT) $(JS_OUTPUT)

$(WASM_OUTPUT) $(JS_OUTPUT): $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE)
	@[ -d docs ] || mkdir -p docs
	@[ -d ${WASMDIR} ] || mkdir -p ${WASMDIR}
	emcc $(EMCCFLAGS) -o docs/rain.html $(STORM_WASM_SOURCE) $(BOW_WASM_SOURCE)
	mv docs/rain.js $(JS_OUTPUT)
	cp $(WASM_OUTPUT) $(JS_OUTPUT) ${WASMDIR}
	rm docs/rain.html

link: 
	@ln -sf rain/bin/rainsum

.PHONY: install

install: rainsum
	cp $(BUILDDIR)/rainsum /usr/local/bin/

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(BUILDDIR) rainsum $(WASMDIR) js/node_modules scripts/node_modules $(WASM_OUTPUT) $(JS_OUTPUT)


