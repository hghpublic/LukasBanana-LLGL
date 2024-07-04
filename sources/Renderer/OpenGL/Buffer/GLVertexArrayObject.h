/*
 * GLVertexArrayObject.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_GL_VERTEX_ARRAY_OBJECT_H
#define LLGL_GL_VERTEX_ARRAY_OBJECT_H


#include "../OpenGL.h"
#include <LLGL/Container/ArrayView.h>


namespace LLGL
{


struct VertexAttribute;
class GLStateManager;

struct GLVertexAttribute
{
    GLuint      buffer;
    GLuint      index;
    GLint       size;
    GLenum      type;
    GLboolean   normalized;
    GLsizei     stride;
    GLsizeiptr  offsetPtrSized;
    GLuint      divisor; // for use with glVertexAttribDivisor()
    bool        isInteger; // meta data for use with glVertexAttribIPointer()
};

// Converts the specified vertex attribute into a GL specific attributes.
void GLConvertVertexAttrib(GLVertexAttribute& dst, const VertexAttribute& src, GLuint srcBuffer);

// Wrapper class for an OpenGL Vertex-Array-Object (VAO), for GL 3.0+.
class GLVertexArrayObject
{

    public:

        // Release VAO from GL context.
        void Release();

        // Builds the specified attribute using a 'glVertexAttrib*Pointer' function.
        void BuildVertexLayout(const ArrayView<GLVertexAttribute>& attributes);

        // Returns the ID of the hardware vertex-array-object (VAO)
        inline GLuint GetID() const
        {
            return id_;
        }

    private:

        void BuildVertexAttribute(const GLVertexAttribute& attribute);

    private:

        GLuint id_ = 0; //!< Vertex array object ID.

};


} // /namespace LLGL


#endif



// ================================================================================
