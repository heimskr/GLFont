#pragma once

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>

#include <iostream>

#ifndef CHECKGL
#define CHECKGL do { if (auto err = glGetError()) { std::cerr << "\e[31mError at " << __FILE__ << ':' << __LINE__ << ": " << gluErrorString(err) << "\e[39m\n"; } } while(0);
#endif

class GLUtils {
public:
    GLUtils();
    ~GLUtils();

    static void loadShader(const char *shaderSource, GLenum shaderType, GLuint &programId);
};

