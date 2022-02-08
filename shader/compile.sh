#!/bin/sh
glslc -fshader-stage=vert default.vert.glsl -o default.vert.spv
glslc -fshader-stage=frag default.frag.glsl -o default.frag.spv
