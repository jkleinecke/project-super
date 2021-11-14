
#include "ps_renderer_opengl.h"

global u32 vao;
global u32 vbo;
global u32 shaderProgram;

void init_test_gl(opengl& gl)
{

    //
    // Just some simple OpenGL test init code
    //
    // TODO(james): remove this code

    const char* szVertShader = "#version 330 core\n"
        "layout (location=0) in vec3 aPos;"
        "void main() {"
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);"
        "}";

    const char* szFragShader = "#version 330 core\n"
        "out vec4 out_Color;"
        "void main(void) {"
        "   out_Color = vec4(1.0f, 0.5f, 0.2f, 1.0);"
        "}";

    r32 vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    } ;
    
    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);

    // TODO(james): add an element buffer to index the vertices
    gl.glGenBuffers(1, &vbo);
    gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void*)0);
    gl.glEnableVertexAttribArray(0);

    u32 vertexShader;
    vertexShader = gl.glCreateShader(GL_VERTEX_SHADER);
    gl.glShaderSource(vertexShader, 1, &szVertShader, 0);
    gl.glCompileShader(vertexShader);

    b32 success;
    gl.glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        char infoLog[512];
        gl.glGetShaderInfoLog(vertexShader, sizeof(infoLog), 0, infoLog);
        LOG_ERROR("OpenGL Vertex Shader compile failed! %s", infoLog);
    }

    u32 fragShader;
    fragShader = gl.glCreateShader(GL_FRAGMENT_SHADER);
    gl.glShaderSource(fragShader, 1, &szFragShader, 0);
    gl.glCompileShader(fragShader);

    gl.glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        char infoLog[512];
        gl.glGetShaderInfoLog(fragShader, sizeof(infoLog), 0, infoLog);
        LOG_ERROR("OpenGL Vertex Shader compile failed! %s", infoLog);
    }

    shaderProgram = gl.glCreateProgram();

    gl.glAttachShader(shaderProgram, vertexShader);
    gl.glAttachShader(shaderProgram, fragShader);
    gl.glLinkProgram(shaderProgram);

    gl.glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success)
    {
        char infoLog[512];
        gl.glGetProgramInfoLog(shaderProgram, sizeof(infoLog), 0, infoLog);
        LOG_ERROR("OpenGL Shader Program link failed! %s", infoLog);
    }

    gl.glUseProgram(shaderProgram);

    gl.glDetachShader(shaderProgram, vertexShader);
    gl.glDetachShader(shaderProgram, fragShader);

    gl.glDeleteShader(vertexShader);
    gl.glDeleteShader(fragShader);
}

internal void
RenderOpenGL(opengl& gl)
{
    gl.glUseProgram(shaderProgram);
    gl.glBindVertexArray(vao);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}