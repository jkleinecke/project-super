
#include "ps_renderer_opengl.h"

global GLuint vao;
global GLuint vbo;

global gl_program g_Program;

struct vertex
{
    v3 position;
};

internal GLuint
OpenGLCreateProgram(opengl& gl, 
    const char* defines, const char* headers,
    const char* szVertexShader, const char* szFragmentShader,
     gl_program& result)
{
    const GLchar* vertexShaderSource[] = {
        defines,
        headers,
        szVertexShader
    };

    GLuint vertexShader;
    vertexShader = gl.glCreateShader(GL_VERTEX_SHADER);
    gl.glShaderSource(vertexShader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, 0);
    gl.glCompileShader(vertexShader);

    const GLchar* fragmentShaderSource[] = {
        defines,
        headers,
        szFragmentShader
    };

    GLuint fragShader;
    fragShader = gl.glCreateShader(GL_FRAGMENT_SHADER);
    gl.glShaderSource(fragShader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, 0);
    gl.glCompileShader(fragShader);

    GLuint programId = gl.glCreateProgram();
    result.id = programId;

    gl.glAttachShader(programId, vertexShader);
    gl.glAttachShader(programId, fragShader);
    gl.glLinkProgram(programId);

    gl.glValidateProgram(programId);

    GLint linked;
    gl.glGetProgramiv(programId, GL_LINK_STATUS, &linked);
    if(!linked)
    {
        GLsizei ignored;
        char vertErrors[4096];
        char fragErrors[4096];
        char programErrors[4096];
        gl.glGetShaderInfoLog(vertexShader, sizeof(vertErrors), &ignored, vertErrors);
        gl.glGetShaderInfoLog(fragShader, sizeof(fragErrors), &ignored, fragErrors);
        gl.glGetProgramInfoLog(programId, sizeof(programErrors), &ignored, programErrors);
        LOG_ERROR(
            "Failed to create opengl program!\n"
            "Vertex Errors: %s\n\n"
            "Fragment Errors: %s\n\n"
            "Program Errors: %s",
            vertErrors, fragErrors, programErrors
            );
        ASSERT(false);
    }

    gl.glUseProgram(programId);

    gl.glDetachShader(programId, vertexShader);
    gl.glDetachShader(programId, fragShader);

    gl.glDeleteShader(vertexShader);
    gl.glDeleteShader(fragShader);

    result.idVertP = gl.glGetAttribLocation(programId, "vertP");

    return programId;
}

internal inline b32
IsVertexAttribValid(GLuint attrib)
{
    b32 result = (attrib != -1);
    return result;
}

void init_test_gl(opengl& gl)
{
    const char* version = "#version 330 core";

    const char* szVertShader = R"FOO(
        layout (location=0) in vec3 vertP;
        void main() {
           gl_Position = vec4(vertP.x, vertP.y, vertP.z, 1.0);
        }
        )FOO";

    const char* szFragShader = R"FOO(
        out vec4 out_Color;
        void main(void) {
           out_Color = vec4(1.0f, 0.5f, 0.2f, 1.0);
        }
        )FOO";

    //
    // Just some simple OpenGL test init code
    //
    // TODO(james): remove this code

    vertex vertices[] = {
        { Vec3(-0.5f, -0.5f, 0.0f) },
        { Vec3(0.5f, -0.5f, 0.0f) },
        { Vec3(0.0f,  0.5f, 0.0f) },
    };
   
    OpenGLCreateProgram(gl, version, "", szVertShader, szFragShader, g_Program);    
    
    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);

    // TODO(james): add an element buffer to index the vertices
    gl.glGenBuffers(1, &vbo);
    gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    if(IsVertexAttribValid(g_Program.idVertP))
    {
        gl.glVertexAttribPointer(g_Program.idVertP, 3, GL_FLOAT, GL_FALSE, sizeof(vertex),  OffsetOf(vertex, position));
        gl.glEnableVertexAttribArray(g_Program.idVertP);
    }
}

internal void
OpenGLRender(opengl& gl)
{
    gl.glUseProgram(g_Program.id);
    gl.glBindVertexArray(vao);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}