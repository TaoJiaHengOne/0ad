#include <GL/glext.h>
#if OS_WIN
# include <GL/wglext.h>
#endif

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

// GL_ARB_shader_objects
FUNC2(void, glDeleteObjectARB, glDeleteObject, "2.0", (GLhandleARB obj))
FUNC2(GLhandleARB, glGetHandleARB, glGetHandle, "2.0", (GLenum pname))
FUNC2(void, glDetachObjectARB, glDetachObject, "2.0", (GLhandleARB containerObj, GLhandleARB attachedObj))
FUNC2(GLhandleARB, glCreateShaderObjectARB, glCreateShaderObject, "2.0", (GLenum shaderType))
FUNC2(void, glShaderSourceARB, glShaderSource, "2.0", (GLhandleARB shaderObj, GLsizei count, const char **string, const GLint *length))
FUNC2(void, glCompileShaderARB, glCompileShader, "2.0", (GLhandleARB shaderObj))
FUNC2(GLhandleARB, glCreateProgramObjectARB, glCreateProgramObject, "2.0", (void))
FUNC2(void, glAttachObjectARB, glAttachObject, "2.0", (GLhandleARB containerObj, GLhandleARB obj))
FUNC2(void, glLinkProgramARB, glLinkProgram, "2.0", (GLhandleARB programObj))
FUNC2(void, glUseProgramObjectARB, glUseProgramObject, "2.0", (GLhandleARB programObj))
FUNC2(void, glValidateProgramARB, glValidateProgram, "2.0", (GLhandleARB programObj))
FUNC2(void, glUniform1fARB, glUniform1f, "2.0", (GLint location, GLfloat v0))
FUNC2(void, glUniform2fARB, glUniform2f, "2.0", (GLint location, GLfloat v0, GLfloat v1))
FUNC2(void, glUniform3fARB, glUniform3f, "2.0", (GLint location, GLfloat v0, GLfloat v1, GLfloat v2))
FUNC2(void, glUniform4fARB, glUniform4f, "2.0", (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3))
FUNC2(void, glUniform1iARB, glUniform1i, "2.0", (GLint location, GLint v0))
FUNC2(void, glUniform2iARB, glUniform2i, "2.0", (GLint location, GLint v0, GLint v1))
FUNC2(void, glUniform3iARB, glUniform3i, "2.0", (GLint location, GLint v0, GLint v1, GLint v2))
FUNC2(void, glUniform4iARB, glUniform4i, "2.0", (GLint location, GLint v0, GLint v1, GLint v2, GLint v3))
FUNC2(void, glUniform1fvARB, glUniform1fv, "2.0", (GLint location, GLsizei count, const GLfloat *value))
FUNC2(void, glUniform2fvARB, glUniform2fv, "2.0", (GLint location, GLsizei count, const GLfloat *value))
FUNC2(void, glUniform3fvARB, glUniform3fv, "2.0", (GLint location, GLsizei count, const GLfloat *value))
FUNC2(void, glUniform4fvARB, glUniform4fv, "2.0", (GLint location, GLsizei count, const GLfloat *value))
FUNC2(void, glUniform1ivARB, glUniform1iv, "2.0", (GLint location, GLsizei count, const GLint *value))
FUNC2(void, glUniform2ivARB, glUniform2iv, "2.0", (GLint location, GLsizei count, const GLint *value))
FUNC2(void, glUniform3ivARB, glUniform3iv, "2.0", (GLint location, GLsizei count, const GLint *value))
FUNC2(void, glUniform4ivARB, glUniform4iv, "2.0", (GLint location, GLsizei count, const GLint *value))
FUNC2(void, glUniformMatrix2fvARB, glUniformMatrix2fv, "2.0", (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value))
FUNC2(void, glUniformMatrix3fvARB, glUniformMatrix3fv, "2.0", (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value))
FUNC2(void, glUniformMatrix4fvARB, glUniformMatrix4fv, "2.0", (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value))
FUNC2(void, glGetObjectParameterfvARB, glGetObjectParameterfv, "2.0", (GLhandleARB obj, GLenum pname, GLfloat *params))
FUNC2(void, glGetObjectParameterivARB, glGetObjectParameteriv, "2.0", (GLhandleARB obj, GLenum pname, GLint *params))
FUNC2(void, glGetInfoLogARB, glGetInfoLog, "2.0", (GLhandleARB obj, GLsizei maxLength, GLsizei *length, char *infoLog))
FUNC2(void, glGetAttachedObjectsARB, glGetAttachedObjects, "2.0", (GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj))
FUNC2(GLint, glGetUniformLocationARB, glGetUniformLocation, "2.0", (GLhandleARB programObj, const char *name))
FUNC2(void, glGetActiveUniformARB, glGetActiveUniform, "2.0", (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, char *name))
FUNC2(void, glGetUniformfvARB, glGetUniformfv, "2.0", (GLhandleARB programObj, GLint location, GLfloat *params))
FUNC2(void, glGetUniformivARB, glGetUniformiv, "2.0", (GLhandleARB programObj, GLint location, GLint *params))
FUNC2(void, glGetShaderSourceARB, glGetShaderSource, "2.0", (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source))


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
