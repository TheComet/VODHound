#include "video-ffmpeg/gfx.h"
#include "video-ffmpeg/canvas.h"

#include "vh/log.h"
#include "vh/mem.h"

#include <epoxy/gl.h>

struct vertex
{
    GLfloat pos[2];
};

static const struct vertex quad_vertices[6] = {
    {{-1, -1}},
    {{-1,  1}},
    {{ 1, -1}},
    {{ 1,  1}},
};
static const char* attr_bindings[] = {
    "v_position",
    NULL
};
static const char* vs_quad = "\
//precision mediump float;\n\
attribute vec2 v_position;\n\
varying vec2 f_texcoord;\n\
uniform vec2 u_aspect;\n\
uniform vec2 u_offset;\n\
void main() {\
    // map [-1, 1] to texture coordinate [0, 1]\n\
    f_texcoord = v_position * vec2(1.0, -1.0) * 0.5 + 0.5;\n\
    f_texcoord = (f_texcoord + u_offset) * u_aspect;\n\
    gl_Position = vec4(v_position, 0.0, 1.0);\n\
}";
static const char* fs_quad = "\
//precision mediump float;\n\
varying vec2 f_texcoord;\n\
uniform sampler2D s_texture;\n\
void main() {\n\
    vec3 col = texture2D(s_texture, f_texcoord).rgb;\n\
    gl_FragColor = vec4(col, 1.0);\n\
}";

struct gfx
{
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLuint texture;
    GLuint u_aspect;
    GLuint u_offset;
    GLuint s_texture;

    int texture_width, texture_height;
};

static GLuint gl_load_shader_type(const GLchar* code, GLenum type)
{
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if (shader == 0)
    {
        log_err("GL Error %d: glCreateShader() failed\n", glGetError());
        return 0;
    }

    glShaderSource(shader, 1, &code, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint info_len = 0;
        log_err("GL Error %d: glCompileShader() failed\n", glGetError());
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* error = mem_alloc(info_len);
            glGetShaderInfoLog(shader, info_len, NULL, error);
            log_err("%.*s\n", info_len, error);
            mem_free(error);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint gl_load_shader(const GLchar* vs, const GLchar* fs, const char* attribute_bindings[])
{
    int i;
    GLuint program;
    GLint linked;

    program = glCreateProgram();
    if (program == 0)
    {
        log_err("GL Error %d: glCreateProgram() failed\n", glGetError());
        goto create_program_failed;
    }

    GLuint vs_shader = gl_load_shader_type(vs, GL_VERTEX_SHADER);
    if (vs_shader == 0)
        goto load_vs_shader_failed;
    glAttachShader(program, vs_shader);
    glDeleteShader(vs_shader);

    GLuint fs_shader = gl_load_shader_type(fs, GL_FRAGMENT_SHADER);
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
        log_err("GL Error %d: glLinkProgram() failed\n", glGetError());
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* error = mem_alloc(info_len);
            glGetProgramInfoLog(program, info_len, NULL, error);
            log_err("%.*s\n", info_len, error);
            mem_free(error);
            goto link_program_failed;
        }
    }

    return program;

    link_program_failed   :
    load_vs_shader_failed :
    load_fs_shader_failed : glDeleteProgram(program);
    create_program_failed : return 0;
}

struct gfx*
gfx_create(void)
{
    struct gfx* gfx = mem_alloc(sizeof *gfx);
    if (gfx == NULL)
        return NULL;

    /* VAO */
    glGenVertexArrays(1, &gfx->vao);
    glBindVertexArray(gfx->vao);

    /* Set up quad mesh */
    glGenBuffers(1, &gfx->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    /* Shader never changes */
    gfx->program = gl_load_shader(vs_quad, fs_quad, attr_bindings);
    gfx->u_offset = glGetUniformLocation(gfx->program, "u_offset");
    gfx->u_aspect = glGetUniformLocation(gfx->program, "u_aspect");
    gfx->s_texture = glGetUniformLocation(gfx->program, "s_texture");

    /* Texture is (re)created in set_frame(), because it might resize */
    gfx->texture = 0;

    return gfx;
}

void
gfx_destroy(struct gfx* gfx)
{
    if (gfx->texture)
        glDeleteTextures(1, &gfx->texture);

    glDeleteProgram(gfx->program);
    glDeleteBuffers(1, &gfx->vbo);
    glDeleteVertexArrays(1, &gfx->vao);

    mem_free(gfx);
}

void
gfx_set_frame(struct gfx* gfx, int width, int height, const void* rgb24)
{
    if (rgb24 == NULL)
    {
        if (gfx->texture)
        {
            glDeleteTextures(1, &gfx->texture);
            gfx->texture = 0;
        }
        return;
    }

    /* Create a new texture if the dimensions change or if there is no existing texture */
    if (gfx->texture == 0 || gfx->texture_width != width || gfx->texture_height != height)
    {
        GLfloat border_color[] = {0.1, 0.1, 0.1, 1.0};
        glGenTextures(1, &gfx->texture);
        glBindTexture(GL_TEXTURE_2D, gfx->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, rgb24);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else  /* Update the existing texture */
    {
        glBindTexture(GL_TEXTURE_2D, gfx->texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
            GL_RGBA, GL_UNSIGNED_BYTE, rgb24);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    gfx->texture_width = width;
    gfx->texture_height = height;
}

void
gfx_render(struct gfx* gfx, int canvas_width, int canvas_height)
{
    GLfloat offsetx = 0.0, offsety = 0.0, aspectx = 1.0, aspecty = 1.0;
    GLfloat canvas_ar = (GLfloat)canvas_width / (GLfloat)canvas_height;
    GLfloat texture_ar = (GLfloat)gfx->texture_width / (GLfloat)gfx->texture_height;
    if (canvas_ar > texture_ar)
    {
        offsetx = ((GLfloat)canvas_height * texture_ar - canvas_width) / 2.0 / canvas_width;
        aspectx = canvas_ar / texture_ar;
    }
    else if (canvas_ar < texture_ar)
    {
        offsety = ((GLfloat)canvas_width / texture_ar - canvas_height) / 2.0 / canvas_height;
        aspecty = texture_ar / canvas_ar;
    }

    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    /* If set_frame() was called with NULL, the texture will be deleted. In this
     * case just clear */
    if (gfx->texture == 0)
        return;

    /* bind quad mesh */
    glBindVertexArray(gfx->vao);

    /* prepare shader */
    glUseProgram(gfx->program);
    glUniform2f(gfx->u_offset, offsetx, offsety);
    glUniform2f(gfx->u_aspect, aspectx, aspecty);

    /* bind texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gfx->texture);
    glUniform1i(gfx->s_texture, 0);

    /* Draw */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* Unbind */
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindVertexArray(0);
}
