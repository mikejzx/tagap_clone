# Kept accidentally running 'make' here so this'll have to do

SRCS_VERT=$(shell find -L . -name '*.vert.glsl' | grep -P '.*\.glsl$$')
OUTS_VERT=$(patsubst %.vert.glsl,%.vert.spv,$(SRCS_VERT))
SRCS_FRAG=$(shell find -L . -name '*.frag.glsl' | grep -P '.*\.glsl$$')
OUTS_FRAG=$(patsubst %.frag.glsl,%.frag.spv,$(SRCS_FRAG))

all: $(OUTS_VERT) $(OUTS_FRAG)

%.vert.spv: %.vert.glsl Makefile
	glslc -fshader-stage=vert $< -o $@

%.frag.spv: %.frag.glsl Makefile
	glslc -fshader-stage=frag $< -o $@
