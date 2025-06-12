#include <jni.h>
#include <GLES2/gl2.h>
#include "common/common.h"

// Vertex Shader
const static char *vShaderSrc =
        "attribute vec4 vPosition;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  color=vPosition.xyz;\n"
        "  gl_Position = vPosition;\n"
        "}";

// Fragment Shader
const static char *fShaderSrc =
        "precision mediump float;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(color.x,color.y, 1.0, 1.0);\n"
        "}";

static GLuint program;
static GLuint vbo;

static void check_compilation_error(GLuint shader) {
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        LOG("Error compiling shader: %s", infoLog);
    }
}

static void initGL(void) {
    // Triangle vertices
    GLfloat vertices[] = {
            0.0f, 0.5f,
            -0.5f, -0.5f,
            0.5f, -0.5f,
    };
    // Compile shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertexShader, 1, &vShaderSrc, NULL);
    glCompileShader(vertexShader);
    check_compilation_error(vertexShader);
    glShaderSource(fragShader, 1, &fShaderSrc, NULL);
    glCompileShader(fragShader);
    check_compilation_error(fragShader);
    // Link program
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    // Create VBO
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
}

static void renderFrame(void) {
    GLint posAttrib = glGetAttribLocation(program, "vPosition");
    glClearColor(0.0f, 0.0f, 0.0f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_init(JNIEnv *env, __attribute__((unused)) jobject thiz) {
    initGL();
}

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_destroy(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jobject thiz) {

}

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_render(JNIEnv *env, jclass clazz) {
    renderFrame();
}