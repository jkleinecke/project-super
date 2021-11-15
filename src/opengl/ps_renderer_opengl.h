#pragma once
#ifndef PS_RENDERER_OPENGL_H
#define PS_RENDERER_OPENGL_H


#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

// GL 1.5
typedef void GLAPI type_glGenBuffers(GLsizei n, GLuint *buffers);
typedef void GLAPI type_glBindBuffer(GLenum target, GLuint buffer);
typedef void GLAPI type_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
// GL 2.0
typedef void GLAPI type_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLint GLAPI type_glGetAttribLocation(GLuint program, const GLchar *name);
typedef GLint GLAPI type_glGetUniformLocation(GLuint program, const GLchar *name);
typedef GLuint GLAPI type_glCreateShader(GLenum type);
typedef void GLAPI type_glDeleteShader(GLuint shader);
typedef void GLAPI type_glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void GLAPI type_glCompileShader(GLuint shader);
typedef GLuint GLAPI type_glCreateProgram();
typedef void GLAPI type_glAttachShader(GLuint program, GLuint shader);
typedef void GLAPI type_glDetachShader(GLuint program, GLuint shader);
typedef void GLAPI type_glLinkProgram(GLuint program);
typedef void GLAPI type_glValidateProgram(GLuint program);
typedef void GLAPI type_glUseProgram(GLuint program);
typedef void GLAPI type_glGetProgramiv(GLuint program, GLenum pname, GLint *params);
typedef void GLAPI type_glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
typedef void GLAPI type_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void GLAPI type_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
// GL 3.0
typedef void GLAPI type_glGenVertexArrays(GLsizei n, GLuint *arrays);
typedef void GLAPI type_glBindVertexArray(GLuint array);
typedef void GLAPI type_glEnableVertexAttribArray(GLuint array);

#define GLFunction(name) type_##name *name

struct opengl
{
    // TODO(james): add more relevant information, like render settings and what-not

    // GL 1.5
    GLFunction(glGenBuffers);
    GLFunction(glBindBuffer);
    GLFunction(glBufferData);
    
    // GL 2.0
    GLFunction(glVertexAttribPointer);
    GLFunction(glGetAttribLocation);
    GLFunction(glGetUniformLocation);
    GLFunction(glCreateShader);
    GLFunction(glDeleteShader);
    GLFunction(glShaderSource);
    GLFunction(glCompileShader);
    GLFunction(glCreateProgram);
    GLFunction(glAttachShader);
    GLFunction(glDetachShader);
    GLFunction(glLinkProgram);
    GLFunction(glValidateProgram);
    GLFunction(glUseProgram);
    GLFunction(glGetProgramiv);
    GLFunction(glGetShaderiv);
    GLFunction(glGetShaderInfoLog);
    GLFunction(glGetProgramInfoLog);
    
    // GL 3.0
    GLFunction(glGenVertexArrays);
    GLFunction(glBindVertexArray);
    GLFunction(glEnableVertexAttribArray);
};

struct gl_program
{
    GLuint id;

    GLint idSampler;

    GLint idVertP;
    GLint idVertUV;
    GLint idVertColor;
};

#endif