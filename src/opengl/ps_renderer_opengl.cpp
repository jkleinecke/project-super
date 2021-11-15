
#include "ps_renderer_opengl.h"

global GLuint vao;
global GLuint vbo;
global GLuint ebo;
global GLuint textureId;

global gl_program g_Program;

struct vertex
{
    v4 position;
    v2 uv;
    v4 color;
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
    result.idVertUV = gl.glGetAttribLocation(programId, "vertUV");
    result.idVertColor = gl.glGetAttribLocation(programId, "vertColor");

    result.idSampler = gl.glGetUniformLocation(programId, "TextureSampler");

    return programId;
}

internal inline b32
IsVertexAttribValid(GLuint attrib)
{

    b32 result = (attrib != -1);
    return result;
}

void init_test_gl(opengl& gl, GraphicsContext& graphics)
{
    const char* version = "#version 330 core";

    const char* szVertShader = R"FOO(
        in vec4 vertP;
        in vec2 vertUV;
        in vec4 vertColor;

        smooth out vec2 FragUV;
        smooth out vec4 FragColor;

        void main() {
           gl_Position = vec4(vertP.xyz, 1.0);
           FragColor = vertColor;
           FragUV = vertUV;
        }
        )FOO";

    const char* szFragShader = R"FOO(
        uniform sampler2D TextureSampler;
        smooth in vec4 FragColor;
        smooth in vec2 FragUV;
        out vec4 out_Color;
        void main(void) {
           out_Color = texture( TextureSampler, FragUV );
        }
        )FOO";

    //
    // Just some simple OpenGL test init code
    //
    // TODO(james): remove this code

    vertex vertices[] = {
        // position, texCoords, color
        { Vec4(-1.0f, 1.0f, 0.0f, 0.0f), Vec2(0.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { Vec4(-1.0f, -1.0f, 0.0f, 0.0f), Vec2(0.0f, 0.0f), Vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { Vec4(1.0f,  -1.0f, 0.0f, 0.0f), Vec2(1.0f, 0.0f), Vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { Vec4(1.0f, 1.0f, 0.0f, 0.0f), Vec2(1.0f, 1.0f) },
    };

    u16 indexes[] = {
        0,1,2,
        0,2,3
    };
   
    OpenGLCreateProgram(gl, version, "", szVertShader, szFragShader, g_Program);    

    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);

    // TODO(james): add an element buffer to index the vertices
    gl.glGenBuffers(1, &vbo);
    gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    gl.glGenBuffers(1, &ebo);
    gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), indexes, GL_STATIC_DRAW);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, graphics.buffer_width, graphics.buffer_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, graphics.buffer);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if(IsVertexAttribValid(g_Program.idVertP))
    {
        gl.glVertexAttribPointer(g_Program.idVertP, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OffsetOf(vertex, position));
        gl.glEnableVertexAttribArray(g_Program.idVertP);
    }

    if(IsVertexAttribValid(g_Program.idVertUV))
    {
        gl.glVertexAttribPointer(g_Program.idVertUV, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OffsetOf(vertex, uv));
        gl.glEnableVertexAttribArray(g_Program.idVertUV);
    }

    if(IsVertexAttribValid(g_Program.idVertColor))
    {
        gl.glVertexAttribPointer(g_Program.idVertColor, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OffsetOf(vertex, color));
        gl.glEnableVertexAttribArray(g_Program.idVertColor);
    }
}

internal void
OpenGLRender(opengl& gl, GraphicsContext& graphics)
{
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, graphics.buffer_width, graphics.buffer_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, graphics.buffer);

    gl.glUseProgram(g_Program.id);
    gl.glBindVertexArray(vao);

    // glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void*)0);
}