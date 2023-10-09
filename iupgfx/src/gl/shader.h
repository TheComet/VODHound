#pragma once

#include "gl/api.h"

GLuint gl_load_shader(const char* vs, const char* fs, const char* attribute_bindings[], char** error);
