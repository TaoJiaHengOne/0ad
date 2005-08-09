#include <GL/glext.h>
#if OS_WIN
# include <GL/wglext.h>
#endif
/*
typedef void* HDC;
typedef void* HGLRC;
typedef void* HPBUFFERARB;
*/

/*

FUNC is used for functions that are only extensions.
FUNC2 is used for functions that have been promoted to core features.

The FUNC2 call includes the version of OpenGL in which the extension was promoted,
and the pre- and post-promotion names (e.g. "glBindBufferARB" vs "glBindBuffer").

If the GL driver is advertising a sufficiently high version, we load the promoted
name; otherwise we use the *ARB name. (The spec says:
	"GL implementations of such later revisions should continue to export the name
	 strings of promoted extensions in the EXTENSIONS string, and continue to support
	 the ARB-affixed versions of functions and enumerants as a transition aid."
but some drivers might be stupid/buggy and fail to do that, so we don't just use
the ARB names unconditionally.)

The names are made accessible to engine code only via the ARB name, to make it
obvious that care must be taken (i.e. by being certain that the extension is
actually supported).

*/

// were these defined as real functions in gl.h already?

#ifndef REAL_GL_1_2
// GL_EXT_draw_range_elements / GL1.2:
FUNC2(void, glDrawRangeElementsEXT, glDrawRangeElements, "1.2", (GLenum, GLuint, GLuint, GLsizei, GLenum, GLvoid*))
#endif

#ifndef REAL_GL_1_3
// GL_ARB_multitexture / GL1.3:
FUNC2(void, glMultiTexCoord2fARB, glMultiTexCoord2f, "1.3", (int, float, float))
FUNC2(void, glActiveTextureARB, glActiveTexture, "1.3", (int))
FUNC2(void, glClientActiveTextureARB, glClientActiveTexture, "1.3", (int))
#endif

// GL_ARB_vertex_buffer_object / GL1.5:
FUNC2(void, glBindBufferARB, glBindBuffer, "1.5", (int target, GLuint buffer))
FUNC2(void, glDeleteBuffersARB, glDeleteBuffers, "1.5", (GLsizei n, const GLuint* buffers))
FUNC2(void, glGenBuffersARB, glGenBuffers, "1.5", (GLsizei n, GLuint* buffers))
FUNC2(bool, glIsBufferARB, glIsBuffer, "1.5", (GLuint buffer))
FUNC2(void, glBufferDataARB, glBufferData, "1.5", (int target, GLsizeiptrARB size, const void* data, int usage))
FUNC2(void, glBufferSubDataARB, glBufferSubData, "1.5", (int target, GLintptrARB offset, GLsizeiptrARB size, const void* data))
FUNC2(void, glGetBufferSubDataARB, glGetBufferSubData, "1.5", (int target, GLintptrARB offset, GLsizeiptrARB size, void* data))
FUNC2(void*, glMapBufferARB, glMapBuffer, "1.5", (int target, int access))
FUNC2(bool, glUnmapBufferARB, glUnmapBuffer, "1.5", (int target))
FUNC2(void, glGetBufferParameterivARB, glGetBufferParameteriv, "1.5", (int target, int pname, int* params))
FUNC2(void, glGetBufferPointervARB, glGetBufferPointerv, "1.5", (int target, int pname, void** params))

// GL_ARB_texture_compression / GL1.3
FUNC2(void, glCompressedTexImage3DARB, glCompressedTexImage3D, "1.3", (GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*))
FUNC2(void, glCompressedTexImage2DARB, glCompressedTexImage2D, "1.3", (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*))
FUNC2(void, glCompressedTexImage1DARB, glCompressedTexImage1D, "1.3", (GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const GLvoid*))
FUNC2(void, glCompressedTexSubImage3DARB, glCompressedTexSubImage3D, "1.3", (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid*))
FUNC2(void, glCompressedTexSubImage2DARB, glCompressedTexSubImage2D, "1.3", (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid*))
FUNC2(void, glCompressedTexSubImage1DARB, glCompressedTexSubImage1D, "1.3", (GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const GLvoid*))
FUNC2(void, glGetCompressedTexImageARB, glGetCompressedTexImage, "1.3", (GLenum, GLint, GLvoid*))

#if OS_WIN
// WGL_EXT_swap_control
FUNC(int, wglSwapIntervalEXT, (int))

// WGL_ARB_pbuffer
FUNC(HPBUFFERARB, wglCreatePbufferARB, (HDC, int, int, int, const int*))
FUNC(HDC, wglGetPbufferDCARB, (HPBUFFERARB))
FUNC(int, wglReleasePbufferDCARB, (HPBUFFERARB, HDC))
FUNC(int, wglDestroyPbufferARB, (HPBUFFERARB))
FUNC(int, wglQueryPbufferARB, (HPBUFFERARB, int, int*))

// GL_ARB_pixel_format
FUNC(int, wglGetPixelFormatAttribivARB, (HDC, int, int, unsigned int, const int*, int*))
FUNC(int, wglGetPixelFormatAttribfvARB, (HDC, int, int, unsigned int, const int*, float*))
FUNC(int, wglChoosePixelFormatARB, (HDC, const int *, const float*, unsigned int, int*, unsigned int*))
#endif // OS_WIN
