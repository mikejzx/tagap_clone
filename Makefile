DIRS=\
	bin/dist \
	bin/debug \

SRCS=$(shell find -L src -name '*.c' | grep -P '.*\.c$$')

CFLAGS_ALL=-D_GNU_SOURCE -Wall -std=c99
CFLAGS_RELEASE=$(CFLAGS_ALL) -march=native -mtune=native -O2 -Wpedantic
CFLAGS_DEBUG=$(CFLAGS_ALL) -Og -g -DDEBUG -Wno-missing-braces

LDFLAGS=-lm -lpthread -lvulkan \
	-lSDL2main $(shell pkg-config --libs --cflags sdl2) \
	-Ilib/cglm/include -Llib/cglm/build -l:libcglm.a \
	-Ilib/vma/include -Llib/vma/build/src -lVulkanMemoryAllocator \
	-Ilib/stb_image -Llib/stb_image -l:libstb_image.a

# 'MODE' environment variable used to make distribution build
ifeq ($(MODE),dist)
	CFLAGS=$(CFLAGS_RELEASE)
	MODE=dist
	OUT=bin/tagap
	ENV_VARS=
else
	CFLAGS=$(CFLAGS_DEBUG)
	MODE=debug
	OUT=bin/tagap-debug
	ENV_VARS=VK_INSTANCE_LAYERS=VK_LAYER_MESA_overlay VK_LAYER_MESA_OVERLAY_CONFIG=position=top-left
endif

OBJS=$(patsubst src/%.c,bin/$(MODE)/%.o,$(SRCS))
DEPS=$(patsubst src/%.c,bin/$(MODE)/%.d,$(SRCS))

.PHONY: all clean run debug

all: dirs $(OUT) shaders

dirs: $(DIRS)

run: all
	$(ENV_VARS) ./$(OUT)

# Run with GDB debugger
debug: $(OUT)
	$(ENV_VARS) gdb $(OUT)

# Link the main executable
$(OUT): $(OBJS)
	@echo "== linking ..."
	g++ $(LIB_VMA) $^ -o $@ $(CFLAGS) $(LDFLAGS)

# Include generated dependencies
-include $(DEPS)

# Compile objects with dependencies
bin/$(MODE)/%.o: src/%.c Makefile
	gcc -MMD -MP -c $< -o $@ $(CFLAGS) $(LDFLAGS)

# Make directories we need for build
$(DIRS):
	mkdir $(DIRS)

# Clean binaries
clean:
	rm -f $(shell find bin -type f)

# Compile SPIR-V shaders
shaders:
	@echo "== compiling shaders ..."
	@cd shader && make
