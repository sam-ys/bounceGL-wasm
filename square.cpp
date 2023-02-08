#include <cstring>

#include <GLES3/gl3.h>
#include <EGL/egl.h>

#include "square.hpp"
#include "texture.hpp"

namespace {
    // Render::Square shape and texture vertices
    static const float VERTICES__[] = {
        -0.5f, -0.5f, 0.0f,    0.0f, 0.0f,
        +0.5f, -0.5f, 0.0f,    1.0f, 0.0f,
        +0.5f,  0.5f, 0.0f,    1.0f, 1.0f,
        +0.5f,  0.5f, 0.0f,    1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,    0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,    0.0f, 0.0f,
    };
}

render::Square::Square(const unsigned* taoSrc, unsigned taoCount, unsigned instanceSizeMax)
{
    ::memset(&tao_, 0, sizeof(tao_));
    ::memset(&vbo_, 0, sizeof(vbo_));

    // Copy texture handles
    ::memcpy(tao_.tao, taoSrc, (tao_.size = taoCount) * sizeof(unsigned));

    // Initialize OpenGL buffers
    glGenVertexArrays(1, &vbo_.mesh);
    glBindVertexArray(vbo_.mesh);

    glGenBuffers(1, &vbo_.vertex);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_.vertex);

    // Add vertices
    glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES__), VERTICES__, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // Instancing
    glGenBuffers(1, &vbo_.instance);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_.instance);

    // Null buffer
    glBufferData(GL_ARRAY_BUFFER, instanceSizeMax * 16 * sizeof(float), nullptr, GL_STREAM_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*)(0));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*)(4  * sizeof(float)));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*)(8  * sizeof(float)));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*)(12 * sizeof(float)));

    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render::Square::draw() const
{
    static const unsigned vertexSize = sizeof(VERTICES__) / sizeof(float) / 5;

    // Load textures...
    glBindVertexArray(0);
    glBindVertexArray(vbo_.mesh);
    glBindTexture(GL_TEXTURE_2D, 0);

    unsigned i = 0;
    for ( ; i != tao_.size; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, tao_.tao[i]);
    }

    // glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glPolygonMode(GL_FRONT_AND_BACK, GL_TRIANGLES);
    // glDisable(GL_STENCIL_TEST);

    // Draw...
    glDrawArraysInstanced(GL_TRIANGLES, 0, vertexSize, vbo_.instanceCount);
}

void render::Square::modify(const float* mat, unsigned instanceIndex)
{
    render::modify(vbo_, mat, instanceIndex);
}

void render::Square::modify(const float* mat, unsigned* instanceIndices, unsigned count)
{
    render::modify(vbo_, mat, instanceIndices, count);
}

void render::Square::reset(const float* mat, unsigned count) {
    render::reset(vbo_, mat, count);
}

void render::Square::push_back(const float* mat) {
    render::push_back(vbo_, mat);
}

void render::Square::push_back(const float* mat, unsigned count) {
    render::push_back(vbo_, mat, count);
}
