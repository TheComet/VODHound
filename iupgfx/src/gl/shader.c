#include "gl/shader.h"
#include <stdlib.h>
#include <string.h>

static GLuint gl_load_shader_type(const char* code, GLint length, GLenum type, char** error)
{
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if (shader == 0)
    {
        if (error)
        {
            *error = malloc(sizeof("glCreateShader() failed"));
            strcpy(*error, "glCreateShader() failed");
        }
        return 0;
    }

    glShaderSource(shader, 1, &code, &length);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1 && error)
        {
            *error = malloc(info_len);
            glGetShaderInfoLog(shader, info_len, NULL, *error);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint gl_load_shader(const char* vs, const char* fs, const char* attribute_bindings[], char** error)
{
    int i;
    GLuint program;
    GLint linked;

    program = glCreateProgram();
    if (program == 0)
    {
        if (error)
        {
            *error = malloc(sizeof("glCreateProgram() failed"));
            strcpy(*error, "glCreateShader() failed");
        }
        goto create_program_failed;
    }

    int length;
    GLuint vs_shader = gl_load_shader_type(vs, length, GL_VERTEX_SHADER, error);
    if (vs_shader == 0)
        goto load_vs_shader_failed;
    glAttachShader(program, vs_shader);
    glDeleteShader(vs_shader);

    GLuint fs_shader = gl_load_shader_type(fs, length, GL_FRAGMENT_SHADER, error);
    if (fs_shader == 0)
        goto load_fs_shader_failed;
    glAttachShader(program, fs_shader);
    glDeleteShader(fs_shader);

    for (i = 0; attribute_bindings[i]; ++i)
        glBindAttribLocation(program, i, attribute_bindings[i]);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1 && error)
        {
            *error = malloc(info_len);
            glGetProgramInfoLog(program, info_len, NULL, *error);
            goto link_program_failed;
        }
    }

    return program;

    link_program_failed   :
    load_vs_shader_failed :
    load_fs_shader_failed : glDeleteProgram(program);
    create_program_failed : return 0;
}

