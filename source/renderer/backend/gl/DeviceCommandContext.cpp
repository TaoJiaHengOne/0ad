/* Copyright (C) 2025 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "DeviceCommandContext.h"

#include "ps/CLogger.h"
#include "renderer/backend/gl/Buffer.h"
#include "renderer/backend/gl/Device.h"
#include "renderer/backend/gl/Framebuffer.h"
#include "renderer/backend/gl/Mapping.h"
#include "renderer/backend/gl/PipelineState.h"
#include "renderer/backend/gl/ShaderProgram.h"
#include "renderer/backend/gl/Texture.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Renderer
{

namespace Backend
{

namespace GL
{

namespace
{

bool operator==(const SStencilOpState& lhs, const SStencilOpState& rhs)
{
	return
		lhs.failOp == rhs.failOp &&
		lhs.passOp == rhs.passOp &&
		lhs.depthFailOp == rhs.depthFailOp &&
		lhs.compareOp == rhs.compareOp;
}
bool operator!=(const SStencilOpState& lhs, const SStencilOpState& rhs)
{
	return !operator==(lhs, rhs);
}

bool operator==(
	const CDeviceCommandContext::Rect& lhs,
	const CDeviceCommandContext::Rect& rhs)
{
	return
		lhs.x == rhs.x && lhs.y == rhs.y &&
		lhs.width == rhs.width && lhs.height == rhs.height;
}

bool operator!=(
	const CDeviceCommandContext::Rect& lhs,
	const CDeviceCommandContext::Rect& rhs)
{
	return !operator==(lhs, rhs);
}

void ApplyDepthMask(const bool depthWriteEnabled)
{
	glDepthMask(depthWriteEnabled ? GL_TRUE : GL_FALSE);
}

void ApplyColorMask(const uint8_t colorWriteMask)
{
	glColorMask(
		(colorWriteMask & ColorWriteMask::RED) != 0 ? GL_TRUE : GL_FALSE,
		(colorWriteMask & ColorWriteMask::GREEN) != 0 ? GL_TRUE : GL_FALSE,
		(colorWriteMask & ColorWriteMask::BLUE) != 0 ? GL_TRUE : GL_FALSE,
		(colorWriteMask & ColorWriteMask::ALPHA) != 0 ? GL_TRUE : GL_FALSE);
}

void ApplyStencilMask(const uint32_t stencilWriteMask)
{
	glStencilMask(stencilWriteMask);
}

GLenum BufferTypeToGLTarget(const CBuffer::Type type)
{
	GLenum target = GL_ARRAY_BUFFER;
	switch (type)
	{
	case CBuffer::Type::VERTEX:
		target = GL_ARRAY_BUFFER;
		break;
	case CBuffer::Type::INDEX:
		target = GL_ELEMENT_ARRAY_BUFFER;
		break;
	case CBuffer::Type::UNIFORM:
#if !CONFIG2_GLES
		target = GL_UNIFORM_BUFFER;
#else
		debug_warn("Unsupported buffer type.");
#endif
		break;
	case CBuffer::Type::UPLOAD:
		debug_warn("Unsupported buffer type.");
		break;
	};
	return target;
}

void UploadDynamicBufferRegionImpl(
	const GLenum target, const uint32_t bufferSize,
	const uint32_t dataOffset, const uint32_t dataSize,
	const CDeviceCommandContext::UploadBufferFunction& uploadFunction)
{
	ENSURE(dataOffset < dataSize);
	// Tell the driver that it can reallocate the whole VBO
	glBufferDataARB(target, bufferSize, nullptr, GL_DYNAMIC_DRAW);
	ogl_WarnIfError();

	while (true)
	{
		// (In theory, glMapBufferRange with GL_MAP_INVALIDATE_BUFFER_BIT could be used
		// here instead of glBufferData(..., NULL, ...) plus glMapBuffer(), but with
		// current Intel Windows GPU drivers (as of 2015-01) it's much faster if you do
		// the explicit glBufferData.)
		void* mappedData = glMapBufferARB(target, GL_WRITE_ONLY);
		if (mappedData == nullptr)
		{
			// This shouldn't happen unless we run out of virtual address space
			LOGERROR("glMapBuffer failed");
			break;
		}

		uploadFunction(static_cast<u8*>(mappedData) + dataOffset);

		if (glUnmapBufferARB(target) == GL_TRUE)
			break;

		// Unmap might fail on e.g. resolution switches, so just try again
		// and hope it will eventually succeed
		LOGMESSAGE("glUnmapBuffer failed, trying again...\n");
	}
}

/**
 * In case we don't need a framebuffer content (because of the following clear
 * or overwriting by a shader) we might give a hint to a driver via
 * glInvalidateFramebuffer.
 */
void InvalidateFramebuffer(
	CFramebuffer* framebuffer, const bool color, const bool depthStencil)
{
	GLsizei numberOfAttachments = 0;
	GLenum attachments[8];
	const bool isBackbuffer = framebuffer->GetHandle() == 0;
	if (color && (framebuffer->GetAttachmentMask() & GL_COLOR_BUFFER_BIT))
	{
		if (isBackbuffer)
#if CONFIG2_GLES
			attachments[numberOfAttachments++] = GL_COLOR_EXT;
#else
			attachments[numberOfAttachments++] = GL_COLOR;
#endif
		else
			attachments[numberOfAttachments++] = GL_COLOR_ATTACHMENT0;
	}
	if (depthStencil)
	{
		if (isBackbuffer)
		{
			if (framebuffer->GetAttachmentMask() & GL_DEPTH_BUFFER_BIT)
#if CONFIG2_GLES
				attachments[numberOfAttachments++] = GL_DEPTH_EXT;
#else
				attachments[numberOfAttachments++] = GL_DEPTH;
#endif
			if (framebuffer->GetAttachmentMask() & GL_STENCIL_BUFFER_BIT)
#if CONFIG2_GLES
				attachments[numberOfAttachments++] = GL_STENCIL_EXT;
#else
				attachments[numberOfAttachments++] = GL_STENCIL;
#endif
		}
		else
		{
			if (framebuffer->GetAttachmentMask() & GL_DEPTH_BUFFER_BIT)
				attachments[numberOfAttachments++] = GL_DEPTH_ATTACHMENT;
			if (framebuffer->GetAttachmentMask() & GL_STENCIL_BUFFER_BIT)
				attachments[numberOfAttachments++] = GL_STENCIL_ATTACHMENT;
		}
	}

	if (numberOfAttachments > 0)
	{
#if CONFIG2_GLES
		glDiscardFramebufferEXT(GL_FRAMEBUFFER_EXT, numberOfAttachments, attachments);
#else
		glInvalidateFramebuffer(GL_FRAMEBUFFER_EXT, numberOfAttachments, attachments);
#endif
		ogl_WarnIfError();
	}
}

} // anonymous namespace

// static
std::unique_ptr<CDeviceCommandContext> CDeviceCommandContext::Create(CDevice* device)
{
	std::unique_ptr<CDeviceCommandContext> deviceCommandContext(new CDeviceCommandContext(device));
	deviceCommandContext->m_Framebuffer = device->GetCurrentBackbuffer(
		Renderer::Backend::AttachmentLoadOp::DONT_CARE,
		Renderer::Backend::AttachmentStoreOp::DONT_CARE,
		Renderer::Backend::AttachmentLoadOp::DONT_CARE,
		Renderer::Backend::AttachmentStoreOp::DONT_CARE)->As<CFramebuffer>();
	deviceCommandContext->ResetStates();
	return deviceCommandContext;
}

CDeviceCommandContext::CDeviceCommandContext(CDevice* device)
	: m_Device(device)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	for (BindUnit& unit : m_BoundTextures)
	{
		unit.target = GL_TEXTURE_2D;
		unit.handle = 0;
	}
	for (size_t index = 0; index < m_VertexAttributeFormat.size(); ++index)
	{
		m_VertexAttributeFormat[index].active = false;
		m_VertexAttributeFormat[index].initialized = false;
		m_VertexAttributeFormat[index].bindingSlot = 0;
	}

	for (size_t index = 0; index < m_BoundBuffers.size(); ++index)
	{
		const CBuffer::Type type = static_cast<CBuffer::Type>(index);
		// Currently we don't support upload buffers for GL.
		if (type == CBuffer::Type::UPLOAD)
			continue;
		const GLenum target = BufferTypeToGLTarget(type);
		const GLuint handle = 0;
		m_BoundBuffers[index].first = target;
		m_BoundBuffers[index].second = handle;
	}
}

CDeviceCommandContext::~CDeviceCommandContext() = default;

IDevice* CDeviceCommandContext::GetDevice()
{
	return m_Device;
}

void CDeviceCommandContext::SetGraphicsPipelineState(
	const SGraphicsPipelineStateDesc& pipelineState)
{
	ENSURE(!pipelineState.shaderProgram || m_InsideFramebufferPass);
	SetGraphicsPipelineStateImpl(pipelineState, false);
}

void CDeviceCommandContext::SetGraphicsPipelineState(
	IGraphicsPipelineState* pipelineState)
{
	ENSURE(!pipelineState->GetShaderProgram() || m_InsideFramebufferPass);
	ENSURE(pipelineState);
	SetGraphicsPipelineStateImpl(
		pipelineState->As<CGraphicsPipelineState>()->GetDesc(), false);
}

void CDeviceCommandContext::SetComputePipelineState(
	IComputePipelineState* pipelineState)
{
	ENSURE(m_InsideComputePass);
	ENSURE(pipelineState);
	const SComputePipelineStateDesc& desc = pipelineState->As<CComputePipelineState>()->GetDesc();
	if (m_ComputePipelineStateDesc.shaderProgram != desc.shaderProgram)
	{
		CShaderProgram* currentShaderProgram = nullptr;
		if (m_ComputePipelineStateDesc.shaderProgram)
			currentShaderProgram = m_ComputePipelineStateDesc.shaderProgram->As<CShaderProgram>();
		CShaderProgram* nextShaderProgram = nullptr;
		if (desc.shaderProgram)
			nextShaderProgram = desc.shaderProgram->As<CShaderProgram>();

		if (nextShaderProgram)
			nextShaderProgram->Bind(currentShaderProgram);
		else if (currentShaderProgram)
			currentShaderProgram->Unbind();

		m_ShaderProgram = nextShaderProgram;
	}
}

void CDeviceCommandContext::UploadTexture(
	ITexture* texture, const Format format,
	const void* data, const size_t dataSize,
	const uint32_t level, const uint32_t layer)
{
	UploadTextureRegion(texture, format, data, dataSize,
		0, 0,
		std::max(1u, texture->GetWidth() >> level),
		std::max(1u, texture->GetHeight() >> level),
		level, layer);
}

void CDeviceCommandContext::UploadTextureRegion(
	ITexture* destinationTexture, const Format dataFormat,
	const void* data, const size_t dataSize,
	const uint32_t xOffset, const uint32_t yOffset,
	const uint32_t width, const uint32_t height,
	const uint32_t level, const uint32_t layer)
{
	ENSURE(destinationTexture);
	CTexture* texture = destinationTexture->As<CTexture>();
	ENSURE(texture->GetUsage() & Renderer::Backend::ITexture::Usage::TRANSFER_DST);
	ENSURE(width > 0 && height > 0);
	if (texture->GetType() == CTexture::Type::TEXTURE_2D)
	{
		ENSURE(layer == 0);
		if (texture->GetFormat() == Format::R8G8B8A8_UNORM ||
			texture->GetFormat() == Format::R8G8B8_UNORM ||
#if !CONFIG2_GLES
			texture->GetFormat() == Format::R8_UNORM ||
#endif
			texture->GetFormat() == Format::A8_UNORM)
		{
			ENSURE(texture->GetFormat() == dataFormat);
			size_t bytesPerPixel = 4;
			GLenum pixelFormat = GL_RGBA;
			switch (dataFormat)
			{
			case Format::R8G8B8A8_UNORM:
				break;
			case Format::R8G8B8_UNORM:
				pixelFormat = GL_RGB;
				bytesPerPixel = 3;
				break;
#if !CONFIG2_GLES
			case Format::R8_UNORM:
				pixelFormat = GL_RED;
				bytesPerPixel = 1;
				break;
#endif
			case Format::A8_UNORM:
				pixelFormat = GL_ALPHA;
				bytesPerPixel = 1;
				break;
			case Format::L8_UNORM:
				pixelFormat = GL_LUMINANCE;
				bytesPerPixel = 1;
				break;
			default:
				debug_warn("Unexpected format.");
				break;
			}
			ENSURE(dataSize == width * height * bytesPerPixel);

			ScopedBind scopedBind(this, GL_TEXTURE_2D, texture->GetHandle());
			glTexSubImage2D(GL_TEXTURE_2D, level,
				xOffset, yOffset, width, height,
				pixelFormat, GL_UNSIGNED_BYTE, data);
			ogl_WarnIfError();
		}
		else if (
			texture->GetFormat() == Format::BC1_RGB_UNORM ||
			texture->GetFormat() == Format::BC1_RGBA_UNORM ||
			texture->GetFormat() == Format::BC2_UNORM ||
			texture->GetFormat() == Format::BC3_UNORM)
		{
			ENSURE(xOffset == 0 && yOffset == 0);
			ENSURE(texture->GetFormat() == dataFormat);
			// TODO: add data size check.

			GLenum internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			switch (texture->GetFormat())
			{
			case Format::BC1_RGBA_UNORM:
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				break;
			case Format::BC2_UNORM:
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				break;
			case Format::BC3_UNORM:
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				break;
			default:
				break;
			}

			ScopedBind scopedBind(this, GL_TEXTURE_2D, texture->GetHandle());
			glCompressedTexImage2DARB(GL_TEXTURE_2D, level, internalFormat, width, height, 0, dataSize, data);
			ogl_WarnIfError();
		}
		else
			debug_warn("Unsupported format");
	}
	else if (texture->GetType() == CTexture::Type::TEXTURE_CUBE)
	{
		if (texture->GetFormat() == Format::R8G8B8A8_UNORM)
		{
			ENSURE(texture->GetFormat() == dataFormat);
			ENSURE(level == 0 && layer < 6);
			ENSURE(xOffset == 0 && yOffset == 0 && texture->GetWidth() == width && texture->GetHeight() == height);
			const size_t bpp = 4;
			ENSURE(dataSize == width * height * bpp);

			// The order of layers should be the following:
			//   front, back, top, bottom, right, left
			static const GLenum targets[6] =
			{
				GL_TEXTURE_CUBE_MAP_POSITIVE_X,
				GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
				GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
				GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
				GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
			};

			ScopedBind scopedBind(this, GL_TEXTURE_CUBE_MAP, texture->GetHandle());
			glTexImage2D(targets[layer], level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			ogl_WarnIfError();
		}
		else
			debug_warn("Unsupported format");
	}
	else
		debug_warn("Unsupported type");
}

void CDeviceCommandContext::UploadBuffer(IBuffer* buffer, const void* data, const uint32_t dataSize)
{
	ENSURE(!m_InsideFramebufferPass);
	UploadBufferRegion(buffer, data, 0, dataSize);
}

void CDeviceCommandContext::UploadBuffer(
	IBuffer* buffer, const UploadBufferFunction& uploadFunction)
{
	ENSURE(!m_InsideFramebufferPass);
	UploadBufferRegion(buffer, 0, buffer->GetSize(), uploadFunction);
}

void CDeviceCommandContext::UploadBufferRegion(
	IBuffer* buffer, const void* data, const uint32_t dataOffset, const uint32_t dataSize)
{
	ENSURE(!m_InsideFramebufferPass);
	ENSURE(data);
	ENSURE(dataOffset + dataSize <= buffer->GetSize());
	const GLenum target = BufferTypeToGLTarget(buffer->GetType());
	ScopedBufferBind scopedBufferBind(this, buffer->As<CBuffer>());
	// Uniform buffers is a relatively new feature so we don't need to use a
	// dynamic upload.
	if (buffer->IsDynamic() && buffer->GetType() != IBuffer::Type::UNIFORM)
	{
		UploadDynamicBufferRegionImpl(target, buffer->GetSize(), dataOffset, dataSize, [data, dataSize](u8* mappedData)
		{
			std::memcpy(mappedData, data, dataSize);
		});
	}
	else
	{
		glBufferSubDataARB(target, dataOffset, dataSize, data);
		ogl_WarnIfError();
	}
}

void CDeviceCommandContext::UploadBufferRegion(
	IBuffer* buffer, const uint32_t dataOffset, const uint32_t dataSize,
	const UploadBufferFunction& uploadFunction)
{
	ENSURE(!m_InsideFramebufferPass);
	ENSURE(dataOffset + dataSize <= buffer->GetSize());
	const GLenum target = BufferTypeToGLTarget(buffer->GetType());
	ScopedBufferBind scopedBufferBind(this, buffer->As<CBuffer>());
	ENSURE(buffer->IsDynamic());
	UploadDynamicBufferRegionImpl(target, buffer->GetSize(), dataOffset, dataSize, uploadFunction);
}

void CDeviceCommandContext::InsertTimestampQuery(const uint32_t handle, const bool UNUSED(isScopeBegin))
{
	// GL can have the only one command context so we can call commands on
	// the deivce side.
	m_Device->InsertTimestampQuery(handle);
}

void CDeviceCommandContext::BeginScopedLabel(const char* name)
{
	if (!m_Device->GetCapabilities().debugScopedLabels)
		return;

	++m_ScopedLabelDepth;
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0x0AD, -1, name);
}

void CDeviceCommandContext::EndScopedLabel()
{
	if (!m_Device->GetCapabilities().debugScopedLabels)
		return;

	ENSURE(m_ScopedLabelDepth > 0);
	--m_ScopedLabelDepth;
	glPopDebugGroup();
}

void CDeviceCommandContext::BindTexture(
	const uint32_t unit, const GLenum target, const GLuint handle)
{
	ENSURE(unit < m_BoundTextures.size());
#if CONFIG2_GLES
	ENSURE(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP);
#else
	ENSURE(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_2D_MULTISAMPLE);
#endif
	if (m_ActiveTextureUnit != unit)
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		m_ActiveTextureUnit = unit;
	}
	if (m_BoundTextures[unit].target == target && m_BoundTextures[unit].handle == handle)
		return;
	if (m_BoundTextures[unit].target != target && m_BoundTextures[unit].target && m_BoundTextures[unit].handle)
		glBindTexture(m_BoundTextures[unit].target, 0);
	if (m_BoundTextures[unit].handle != handle)
		glBindTexture(target, handle);
	ogl_WarnIfError();
	m_BoundTextures[unit] = {target, handle};
}

void CDeviceCommandContext::BindBuffer(const IBuffer::Type type, CBuffer* buffer)
{
	ENSURE(!buffer || buffer->GetType() == type);
	if (type == IBuffer::Type::VERTEX)
	{
		if (m_VertexBuffer == buffer)
			return;
		m_VertexBuffer = buffer;
	}
	else if (type == IBuffer::Type::INDEX)
	{
		if (!buffer)
			m_IndexBuffer = nullptr;
		m_IndexBufferData = nullptr;
	}
	const GLenum target = BufferTypeToGLTarget(type);
	const GLuint handle = buffer ? buffer->GetHandle() : 0;
	glBindBufferARB(target, handle);
	ogl_WarnIfError();
	const size_t cacheIndex = static_cast<size_t>(type);
	ENSURE(cacheIndex < m_BoundBuffers.size());
	m_BoundBuffers[cacheIndex].second = handle;
}

void CDeviceCommandContext::OnTextureDestroy(CTexture* texture)
{
	ENSURE(texture);
	for (size_t index = 0; index < m_BoundTextures.size(); ++index)
		if (m_BoundTextures[index].handle == texture->GetHandle())
			BindTexture(index, GL_TEXTURE_2D, 0);
}

void CDeviceCommandContext::Flush()
{
	ENSURE(m_ScopedLabelDepth == 0);
	ENSURE(!m_InsideFramebufferPass);
	ENSURE(!m_InsideComputePass);

	GPU_SCOPED_LABEL(this, "CDeviceCommandContext::Flush");

	ResetStates();

	m_IndexBuffer = nullptr;
	m_IndexBufferData = nullptr;

	for (size_t unit = 0; unit < m_BoundTextures.size(); ++unit)
	{
		if (m_BoundTextures[unit].handle)
			BindTexture(unit, GL_TEXTURE_2D, 0);
	}
	BindBuffer(CBuffer::Type::INDEX, nullptr);
	BindBuffer(CBuffer::Type::VERTEX, nullptr);
}

void CDeviceCommandContext::ResetStates()
{
	SetGraphicsPipelineStateImpl(MakeDefaultGraphicsPipelineStateDesc(), true);
	SetScissors(0, nullptr);
	m_Framebuffer = m_Device->GetCurrentBackbuffer(
		Renderer::Backend::AttachmentLoadOp::DONT_CARE,
		Renderer::Backend::AttachmentStoreOp::DONT_CARE,
		Renderer::Backend::AttachmentLoadOp::DONT_CARE,
		Renderer::Backend::AttachmentStoreOp::DONT_CARE)->As<CFramebuffer>();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_Framebuffer->GetHandle());
	ogl_WarnIfError();
}

void CDeviceCommandContext::SetGraphicsPipelineStateImpl(
	const SGraphicsPipelineStateDesc& pipelineStateDesc, const bool force)
{
	ENSURE(!m_InsidePass);

	if (m_GraphicsPipelineStateDesc.shaderProgram != pipelineStateDesc.shaderProgram)
	{
		CShaderProgram* currentShaderProgram = nullptr;
		if (m_GraphicsPipelineStateDesc.shaderProgram)
		{
			currentShaderProgram =
				static_cast<CShaderProgram*>(m_GraphicsPipelineStateDesc.shaderProgram);
		}
		CShaderProgram* nextShaderProgram = nullptr;
		if (pipelineStateDesc.shaderProgram)
		{
			nextShaderProgram =
				static_cast<CShaderProgram*>(pipelineStateDesc.shaderProgram);
			for (size_t index = 0; index < m_VertexAttributeFormat.size(); ++index)
			{
				const VertexAttributeStream stream = static_cast<VertexAttributeStream>(index);
				m_VertexAttributeFormat[index].active = nextShaderProgram->IsStreamActive(stream);
				m_VertexAttributeFormat[index].initialized = false;
				m_VertexAttributeFormat[index].bindingSlot = std::numeric_limits<uint32_t>::max();
			}
		}
		if (nextShaderProgram)
			nextShaderProgram->Bind(currentShaderProgram);
		else if (currentShaderProgram)
			currentShaderProgram->Unbind();

		m_ShaderProgram = nextShaderProgram;
	}

	const SDepthStencilStateDesc& currentDepthStencilStateDesc = m_GraphicsPipelineStateDesc.depthStencilState;
	const SDepthStencilStateDesc& nextDepthStencilStateDesc = pipelineStateDesc.depthStencilState;
	if (force || currentDepthStencilStateDesc.depthTestEnabled != nextDepthStencilStateDesc.depthTestEnabled)
	{
		if (nextDepthStencilStateDesc.depthTestEnabled)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	}
	if (force || currentDepthStencilStateDesc.depthCompareOp != nextDepthStencilStateDesc.depthCompareOp)
	{
		glDepthFunc(Mapping::FromCompareOp(nextDepthStencilStateDesc.depthCompareOp));
	}
	if (force || currentDepthStencilStateDesc.depthWriteEnabled != nextDepthStencilStateDesc.depthWriteEnabled)
	{
		ApplyDepthMask(nextDepthStencilStateDesc.depthWriteEnabled);
	}

	if (force || currentDepthStencilStateDesc.stencilTestEnabled != nextDepthStencilStateDesc.stencilTestEnabled)
	{
		if (nextDepthStencilStateDesc.stencilTestEnabled)
			glEnable(GL_STENCIL_TEST);
		else
			glDisable(GL_STENCIL_TEST);
	}
	if (force ||
		currentDepthStencilStateDesc.stencilFrontFace != nextDepthStencilStateDesc.stencilFrontFace ||
		currentDepthStencilStateDesc.stencilBackFace != nextDepthStencilStateDesc.stencilBackFace)
	{
		if (nextDepthStencilStateDesc.stencilFrontFace == nextDepthStencilStateDesc.stencilBackFace)
		{
			glStencilOp(
				Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilFrontFace.failOp),
				Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilFrontFace.depthFailOp),
				Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilFrontFace.passOp));
		}
		else
		{
			if (force || currentDepthStencilStateDesc.stencilFrontFace != nextDepthStencilStateDesc.stencilFrontFace)
			{
				glStencilOpSeparate(
					GL_FRONT,
					Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilFrontFace.failOp),
					Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilFrontFace.depthFailOp),
					Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilFrontFace.passOp));
			}
			if (force || currentDepthStencilStateDesc.stencilBackFace != nextDepthStencilStateDesc.stencilBackFace)
			{
				glStencilOpSeparate(
					GL_BACK,
					Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilBackFace.failOp),
					Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilBackFace.depthFailOp),
					Mapping::FromStencilOp(nextDepthStencilStateDesc.stencilBackFace.passOp));
			}
		}
	}
	if (force || currentDepthStencilStateDesc.stencilWriteMask != nextDepthStencilStateDesc.stencilWriteMask)
	{
		ApplyStencilMask(nextDepthStencilStateDesc.stencilWriteMask);
	}
	if (force ||
		currentDepthStencilStateDesc.stencilReference != nextDepthStencilStateDesc.stencilReference ||
		currentDepthStencilStateDesc.stencilReadMask != nextDepthStencilStateDesc.stencilReadMask ||
		currentDepthStencilStateDesc.stencilFrontFace.compareOp != nextDepthStencilStateDesc.stencilFrontFace.compareOp ||
		currentDepthStencilStateDesc.stencilBackFace.compareOp != nextDepthStencilStateDesc.stencilBackFace.compareOp)
	{
		if (nextDepthStencilStateDesc.stencilFrontFace.compareOp == nextDepthStencilStateDesc.stencilBackFace.compareOp)
		{
			glStencilFunc(
				Mapping::FromCompareOp(nextDepthStencilStateDesc.stencilFrontFace.compareOp),
				nextDepthStencilStateDesc.stencilReference,
				nextDepthStencilStateDesc.stencilReadMask);
		}
		else
		{
			glStencilFuncSeparate(GL_FRONT,
				Mapping::FromCompareOp(nextDepthStencilStateDesc.stencilFrontFace.compareOp),
				nextDepthStencilStateDesc.stencilReference,
				nextDepthStencilStateDesc.stencilReadMask);
			glStencilFuncSeparate(GL_BACK,
				Mapping::FromCompareOp(nextDepthStencilStateDesc.stencilBackFace.compareOp),
				nextDepthStencilStateDesc.stencilReference,
				nextDepthStencilStateDesc.stencilReadMask);
		}
	}

	const SBlendStateDesc& currentBlendStateDesc = m_GraphicsPipelineStateDesc.blendState;
	const SBlendStateDesc& nextBlendStateDesc = pipelineStateDesc.blendState;
	if (force || currentBlendStateDesc.enabled != nextBlendStateDesc.enabled)
	{
		if (nextBlendStateDesc.enabled)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}
	if (force ||
		currentBlendStateDesc.srcColorBlendFactor != nextBlendStateDesc.srcColorBlendFactor ||
		currentBlendStateDesc.srcAlphaBlendFactor != nextBlendStateDesc.srcAlphaBlendFactor ||
		currentBlendStateDesc.dstColorBlendFactor != nextBlendStateDesc.dstColorBlendFactor ||
		currentBlendStateDesc.dstAlphaBlendFactor != nextBlendStateDesc.dstAlphaBlendFactor)
	{
		if (nextBlendStateDesc.srcColorBlendFactor == nextBlendStateDesc.srcAlphaBlendFactor &&
			nextBlendStateDesc.dstColorBlendFactor == nextBlendStateDesc.dstAlphaBlendFactor)
		{
			glBlendFunc(
				Mapping::FromBlendFactor(nextBlendStateDesc.srcColorBlendFactor),
				Mapping::FromBlendFactor(nextBlendStateDesc.dstColorBlendFactor));
		}
		else
		{
			glBlendFuncSeparate(
				Mapping::FromBlendFactor(nextBlendStateDesc.srcColorBlendFactor),
				Mapping::FromBlendFactor(nextBlendStateDesc.dstColorBlendFactor),
				Mapping::FromBlendFactor(nextBlendStateDesc.srcAlphaBlendFactor),
				Mapping::FromBlendFactor(nextBlendStateDesc.dstAlphaBlendFactor));
		}
	}

	if (force ||
		currentBlendStateDesc.colorBlendOp != nextBlendStateDesc.colorBlendOp ||
		currentBlendStateDesc.alphaBlendOp != nextBlendStateDesc.alphaBlendOp)
	{
		if (nextBlendStateDesc.colorBlendOp == nextBlendStateDesc.alphaBlendOp)
		{
			glBlendEquation(Mapping::FromBlendOp(nextBlendStateDesc.colorBlendOp));
		}
		else
		{
			glBlendEquationSeparate(
				Mapping::FromBlendOp(nextBlendStateDesc.colorBlendOp),
				Mapping::FromBlendOp(nextBlendStateDesc.alphaBlendOp));
		}
	}

	if (force ||
		currentBlendStateDesc.constant != nextBlendStateDesc.constant)
	{
		glBlendColor(
			nextBlendStateDesc.constant.r,
			nextBlendStateDesc.constant.g,
			nextBlendStateDesc.constant.b,
			nextBlendStateDesc.constant.a);
	}

	if (force ||
		currentBlendStateDesc.colorWriteMask != nextBlendStateDesc.colorWriteMask)
	{
		ApplyColorMask(nextBlendStateDesc.colorWriteMask);
	}

	const SRasterizationStateDesc& currentRasterizationStateDesc =
		m_GraphicsPipelineStateDesc.rasterizationState;
	const SRasterizationStateDesc& nextRasterizationStateDesc =
		pipelineStateDesc.rasterizationState;
	if (force ||
		currentRasterizationStateDesc.polygonMode != nextRasterizationStateDesc.polygonMode)
	{
#if !CONFIG2_GLES
		glPolygonMode(
			GL_FRONT_AND_BACK,
			nextRasterizationStateDesc.polygonMode == PolygonMode::LINE ? GL_LINE : GL_FILL);
#endif
	}

	if (force ||
		currentRasterizationStateDesc.cullMode != nextRasterizationStateDesc.cullMode)
	{
		if (nextRasterizationStateDesc.cullMode == CullMode::NONE)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			if (force || currentRasterizationStateDesc.cullMode == CullMode::NONE)
				glEnable(GL_CULL_FACE);
			glCullFace(nextRasterizationStateDesc.cullMode == CullMode::FRONT ? GL_FRONT : GL_BACK);
		}
	}

	if (force ||
		currentRasterizationStateDesc.frontFace != nextRasterizationStateDesc.frontFace)
	{
		if (nextRasterizationStateDesc.frontFace == FrontFace::CLOCKWISE)
			glFrontFace(GL_CW);
		else
			glFrontFace(GL_CCW);
	}

#if !CONFIG2_GLES
	if (force ||
		currentRasterizationStateDesc.depthBiasEnabled != nextRasterizationStateDesc.depthBiasEnabled)
	{
		if (nextRasterizationStateDesc.depthBiasEnabled)
			glEnable(GL_POLYGON_OFFSET_FILL);
		else
			glDisable(GL_POLYGON_OFFSET_FILL);
	}
	if (force ||
		currentRasterizationStateDesc.depthBiasConstantFactor != nextRasterizationStateDesc.depthBiasConstantFactor ||
		currentRasterizationStateDesc.depthBiasSlopeFactor != nextRasterizationStateDesc.depthBiasSlopeFactor)
	{
		glPolygonOffset(
			nextRasterizationStateDesc.depthBiasSlopeFactor,
			nextRasterizationStateDesc.depthBiasConstantFactor);
	}
#endif

	ogl_WarnIfError();

	m_GraphicsPipelineStateDesc = pipelineStateDesc;
}

void CDeviceCommandContext::BlitFramebuffer(
	IFramebuffer* srcFramebuffer, IFramebuffer* dstFramebuffer,
	const Rect& sourceRegion, const Rect& destinationRegion,
	const Sampler::Filter filter)
{
	ENSURE(!m_InsideFramebufferPass);
	CFramebuffer* destinationFramebuffer = dstFramebuffer->As<CFramebuffer>();
	CFramebuffer* sourceFramebuffer = srcFramebuffer->As<CFramebuffer>();
#if CONFIG2_GLES
	UNUSED2(destinationFramebuffer);
	UNUSED2(sourceFramebuffer);
	UNUSED2(destinationRegion);
	UNUSED2(sourceRegion);
	UNUSED2(filter);
	debug_warn("CDeviceCommandContext::BlitFramebuffer is not implemented for GLES");
#else
	// Source framebuffer should not be backbuffer.
	ENSURE(sourceFramebuffer->GetHandle() != 0);
	ENSURE(destinationFramebuffer != sourceFramebuffer);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, sourceFramebuffer->GetHandle());
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, destinationFramebuffer->GetHandle());
	// TODO: add more check for internal formats.
	glBlitFramebufferEXT(
		sourceRegion.x, sourceRegion.y, sourceRegion.width, sourceRegion.height,
		destinationRegion.x, destinationRegion.y, destinationRegion.width, destinationRegion.height,
		(sourceFramebuffer->GetAttachmentMask() & destinationFramebuffer->GetAttachmentMask()),
		filter == Sampler::Filter::LINEAR ? GL_LINEAR : GL_NEAREST);
	ogl_WarnIfError();
#endif
}

void CDeviceCommandContext::ResolveFramebuffer(
	IFramebuffer* srcFramebuffer, IFramebuffer* dstFramebuffer)
{
	ENSURE(!m_InsideFramebufferPass);
	CFramebuffer* destinationFramebuffer = dstFramebuffer->As<CFramebuffer>();
	CFramebuffer* sourceFramebuffer = srcFramebuffer->As<CFramebuffer>();
	ENSURE(destinationFramebuffer->GetWidth() == sourceFramebuffer->GetWidth());
	ENSURE(destinationFramebuffer->GetHeight() == sourceFramebuffer->GetHeight());
#if CONFIG2_GLES
	UNUSED2(destinationFramebuffer);
	UNUSED2(sourceFramebuffer);
	debug_warn("CDeviceCommandContext::ResolveFramebuffer is not implemented for GLES");
#else
	// Source framebuffer should not be backbuffer.
	ENSURE(sourceFramebuffer->GetHandle() != 0);
	ENSURE(destinationFramebuffer != sourceFramebuffer);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, sourceFramebuffer->GetHandle());
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, destinationFramebuffer->GetHandle());
	glBlitFramebufferEXT(
		0, 0, sourceFramebuffer->GetWidth(), sourceFramebuffer->GetHeight(),
		0, 0, sourceFramebuffer->GetWidth(), sourceFramebuffer->GetHeight(),
		(sourceFramebuffer->GetAttachmentMask() & destinationFramebuffer->GetAttachmentMask()),
		GL_NEAREST);
	ogl_WarnIfError();
#endif
}

void CDeviceCommandContext::ClearFramebuffer(const bool color, const bool depth, const bool stencil)
{
	ENSURE(m_InsideFramebufferPass);
	const bool needsColor = color && (m_Framebuffer->GetAttachmentMask() & GL_COLOR_BUFFER_BIT) != 0;
	const bool needsDepth = depth && (m_Framebuffer->GetAttachmentMask() & GL_DEPTH_BUFFER_BIT) != 0;
	const bool needsStencil = stencil && (m_Framebuffer->GetAttachmentMask() & GL_STENCIL_BUFFER_BIT) != 0;
	GLbitfield mask = 0;
	if (needsColor)
	{
		ApplyColorMask(ColorWriteMask::RED | ColorWriteMask::GREEN | ColorWriteMask::BLUE | ColorWriteMask::ALPHA);
		glClearColor(
			m_Framebuffer->GetClearColor().r,
			m_Framebuffer->GetClearColor().g,
			m_Framebuffer->GetClearColor().b,
			m_Framebuffer->GetClearColor().a);
		mask |= GL_COLOR_BUFFER_BIT;
	}
	if (needsDepth)
	{
		ApplyDepthMask(true);
		mask |= GL_DEPTH_BUFFER_BIT;
	}
	if (needsStencil)
	{
		ApplyStencilMask(std::numeric_limits<uint32_t>::max());
		mask |= GL_STENCIL_BUFFER_BIT;
	}
	glClear(mask);
	ogl_WarnIfError();
	if (needsColor)
		ApplyColorMask(m_GraphicsPipelineStateDesc.blendState.colorWriteMask);
	if (needsDepth)
		ApplyDepthMask(m_GraphicsPipelineStateDesc.depthStencilState.depthWriteEnabled);
	if (needsStencil)
		ApplyStencilMask(m_GraphicsPipelineStateDesc.depthStencilState.stencilWriteMask);
}

void CDeviceCommandContext::BeginFramebufferPass(IFramebuffer* framebuffer)
{
	SetGraphicsPipelineStateImpl(
		MakeDefaultGraphicsPipelineStateDesc(), false);

	ENSURE(!m_InsideFramebufferPass);
	m_InsideFramebufferPass = true;
	ENSURE(framebuffer);
	m_Framebuffer = framebuffer->As<CFramebuffer>();
	ENSURE(m_Framebuffer->GetHandle() == 0 || (m_Framebuffer->GetWidth() > 0 && m_Framebuffer->GetHeight() > 0));
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_Framebuffer->GetHandle());
	ogl_WarnIfError();
	if (m_Device->UseFramebufferInvalidating())
	{
		InvalidateFramebuffer(
			m_Framebuffer,
			m_Framebuffer->GetColorAttachmentLoadOp() != AttachmentLoadOp::LOAD,
			m_Framebuffer->GetDepthStencilAttachmentLoadOp() != AttachmentLoadOp::LOAD);
	}
	const bool needsClearColor =
		m_Framebuffer->GetColorAttachmentLoadOp() == AttachmentLoadOp::CLEAR;
	const bool needsClearDepthStencil =
		m_Framebuffer->GetDepthStencilAttachmentLoadOp() == AttachmentLoadOp::CLEAR;
	if (needsClearColor || needsClearDepthStencil)
	{
		ClearFramebuffer(
			needsClearColor, needsClearDepthStencil, needsClearDepthStencil);
	}
}

void CDeviceCommandContext::EndFramebufferPass()
{
	if (m_Device->UseFramebufferInvalidating())
	{
		InvalidateFramebuffer(
			m_Framebuffer,
			m_Framebuffer->GetColorAttachmentStoreOp() != AttachmentStoreOp::STORE,
			m_Framebuffer->GetDepthStencilAttachmentStoreOp() != AttachmentStoreOp::STORE);
	}
	ENSURE(m_InsideFramebufferPass);
	m_InsideFramebufferPass = false;
	CFramebuffer* framebuffer = m_Device->GetCurrentBackbuffer(
		Renderer::Backend::AttachmentLoadOp::DONT_CARE,
		Renderer::Backend::AttachmentStoreOp::DONT_CARE,
		Renderer::Backend::AttachmentLoadOp::DONT_CARE,
		Renderer::Backend::AttachmentStoreOp::DONT_CARE)->As<CFramebuffer>();
	if (framebuffer->GetHandle() != m_Framebuffer->GetHandle())
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer->GetHandle());
		ogl_WarnIfError();
	}
	m_Framebuffer = framebuffer;

	SetGraphicsPipelineStateImpl(MakeDefaultGraphicsPipelineStateDesc(), false);
}

void CDeviceCommandContext::ReadbackFramebufferSync(
	const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height,
	void* data)
{
	ENSURE(m_Framebuffer);
	glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
	ogl_WarnIfError();
}

void CDeviceCommandContext::SetScissors(const uint32_t scissorCount, const Rect* scissors)
{
	ENSURE(scissorCount <= 1);
	if (scissorCount == 0)
	{
		if (m_ScissorCount != scissorCount)
			glDisable(GL_SCISSOR_TEST);
	}
	else
	{
		if (m_ScissorCount != scissorCount)
			glEnable(GL_SCISSOR_TEST);
		ENSURE(scissors);
		if (m_ScissorCount != scissorCount || m_Scissors[0] != scissors[0])
		{
			m_Scissors[0] = scissors[0];
			glScissor(m_Scissors[0].x, m_Scissors[0].y, m_Scissors[0].width, m_Scissors[0].height);
		}
	}
	ogl_WarnIfError();
	m_ScissorCount = scissorCount;
}

void CDeviceCommandContext::SetViewports(const uint32_t viewportCount, const Rect* viewports)
{
	ENSURE(m_InsideFramebufferPass);
	ENSURE(viewportCount == 1);
	glViewport(viewports[0].x, viewports[0].y, viewports[0].width, viewports[0].height);
	ogl_WarnIfError();
}

void CDeviceCommandContext::SetVertexInputLayout(
	IVertexInputLayout* vertexInputLayout)
{
	ENSURE(vertexInputLayout);
	for (const SVertexAttributeFormat& attribute : vertexInputLayout->As<CVertexInputLayout>()->GetAttributes())
	{
		const uint32_t index = static_cast<uint32_t>(attribute.stream);
		ENSURE(index < m_VertexAttributeFormat.size());
		ENSURE(attribute.bindingSlot < m_VertexAttributeFormat.size());
		if (!m_VertexAttributeFormat[index].active)
			continue;

		m_VertexAttributeFormat[index].format = attribute.format;
		m_VertexAttributeFormat[index].offset = attribute.offset;
		m_VertexAttributeFormat[index].stride = attribute.stride;
		m_VertexAttributeFormat[index].rate = attribute.rate;
		m_VertexAttributeFormat[index].bindingSlot = attribute.bindingSlot;

		m_VertexAttributeFormat[index].initialized = true;
	}
}

void CDeviceCommandContext::SetVertexBuffer(
	const uint32_t bindingSlot, IBuffer* buffer, const uint32_t offset)
{
	ENSURE(buffer);
	ENSURE(buffer->GetType() == IBuffer::Type::VERTEX);
	ENSURE(m_ShaderProgram);
	BindBuffer(buffer->GetType(), buffer->As<CBuffer>());
	for (size_t index = 0; index < m_VertexAttributeFormat.size(); ++index)
	{
		if (!m_VertexAttributeFormat[index].active || m_VertexAttributeFormat[index].bindingSlot != bindingSlot)
			continue;
		ENSURE(m_VertexAttributeFormat[index].initialized);
		const VertexAttributeStream stream = static_cast<VertexAttributeStream>(index);
		m_ShaderProgram->VertexAttribPointer(stream,
			m_VertexAttributeFormat[index].format,
			m_VertexAttributeFormat[index].offset + offset,
			m_VertexAttributeFormat[index].stride,
			m_VertexAttributeFormat[index].rate,
			nullptr);
	}
}

void CDeviceCommandContext::SetVertexBufferData(
	const uint32_t bindingSlot, const void* data, const uint32_t dataSize)
{
	ENSURE(data);
	ENSURE(m_ShaderProgram);
	ENSURE(dataSize > 0);
	BindBuffer(CBuffer::Type::VERTEX, nullptr);
	for (size_t index = 0; index < m_VertexAttributeFormat.size(); ++index)
	{
		if (!m_VertexAttributeFormat[index].active || m_VertexAttributeFormat[index].bindingSlot != bindingSlot)
			continue;
		ENSURE(m_VertexAttributeFormat[index].initialized);
		const VertexAttributeStream stream = static_cast<VertexAttributeStream>(index);
		// We don't know how many vertices will be used in a draw command, so we
		// assume at least one vertex.
		ENSURE(dataSize >= m_VertexAttributeFormat[index].offset + m_VertexAttributeFormat[index].stride);
		m_ShaderProgram->VertexAttribPointer(stream,
			m_VertexAttributeFormat[index].format,
			m_VertexAttributeFormat[index].offset,
			m_VertexAttributeFormat[index].stride,
			m_VertexAttributeFormat[index].rate,
			data);
	}
}

void CDeviceCommandContext::SetIndexBuffer(IBuffer* buffer)
{
	ENSURE(buffer->GetType() == CBuffer::Type::INDEX);
	m_IndexBuffer = buffer->As<CBuffer>();
	m_IndexBufferData = nullptr;
	BindBuffer(CBuffer::Type::INDEX, m_IndexBuffer);
}

void CDeviceCommandContext::SetIndexBufferData(const void* data, const uint32_t dataSize)
{
	ENSURE(dataSize > 0);
	if (m_IndexBuffer)
	{
		BindBuffer(CBuffer::Type::INDEX, nullptr);
		m_IndexBuffer = nullptr;
	}
	m_IndexBufferData = data;
}

void CDeviceCommandContext::BeginPass()
{
	ENSURE(!m_InsidePass);
	m_InsidePass = true;
}

void CDeviceCommandContext::EndPass()
{
	ENSURE(m_InsidePass);
	m_InsidePass = false;
}

void CDeviceCommandContext::Draw(
	const uint32_t firstVertex, const uint32_t vertexCount)
{
	ENSURE(m_ShaderProgram);
	ENSURE(m_InsidePass);
	// Some drivers apparently don't like count = 0 in glDrawArrays here, so skip
	// all drawing in that case.
	if (vertexCount == 0)
		return;
	m_ShaderProgram->AssertPointersBound();
	glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
	ogl_WarnIfError();
}

void CDeviceCommandContext::DrawIndexed(
	const uint32_t firstIndex, const uint32_t indexCount, const int32_t vertexOffset)
{
	ENSURE(m_ShaderProgram);
	ENSURE(m_InsidePass);
	if (indexCount == 0)
		return;
	ENSURE(m_IndexBuffer || m_IndexBufferData);
	ENSURE(vertexOffset == 0);
	if (m_IndexBuffer)
	{
		ENSURE(sizeof(uint16_t) * (firstIndex + indexCount) <= m_IndexBuffer->GetSize());
	}
	m_ShaderProgram->AssertPointersBound();
	// Don't use glMultiDrawElements here since it doesn't have a significant
	// performance impact and it suffers from various driver bugs (e.g. it breaks
	// in Mesa 7.10 swrast with index VBOs).
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT,
		static_cast<const void*>((static_cast<const uint8_t*>(m_IndexBufferData) + sizeof(uint16_t) * firstIndex)));
	ogl_WarnIfError();
}

void CDeviceCommandContext::DrawInstanced(
	const uint32_t firstVertex, const uint32_t vertexCount,
	const uint32_t firstInstance, const uint32_t instanceCount)
{
	ENSURE(m_Device->GetCapabilities().instancing);
	ENSURE(m_ShaderProgram);
	ENSURE(m_InsidePass);
	if (vertexCount == 0 || instanceCount == 0)
		return;
	ENSURE(firstInstance == 0);
	m_ShaderProgram->AssertPointersBound();
#if CONFIG2_GLES
	ENSURE(!m_Device->GetCapabilities().instancing);
	UNUSED2(firstVertex);
	UNUSED2(vertexCount);
	UNUSED2(instanceCount);
#else
	glDrawArraysInstancedARB(GL_TRIANGLES, firstVertex, vertexCount, instanceCount);
#endif
	ogl_WarnIfError();
}

void CDeviceCommandContext::DrawIndexedInstanced(
	const uint32_t firstIndex, const uint32_t indexCount,
	const uint32_t firstInstance, const uint32_t instanceCount,
	const int32_t vertexOffset)
{
	ENSURE(m_Device->GetCapabilities().instancing);
	ENSURE(m_ShaderProgram);
	ENSURE(m_InsidePass);
	ENSURE(m_IndexBuffer || m_IndexBufferData);
	if (indexCount == 0)
		return;
	ENSURE(firstInstance == 0 && vertexOffset == 0);
	if (m_IndexBuffer)
	{
		ENSURE(sizeof(uint16_t) * (firstIndex + indexCount) <= m_IndexBuffer->GetSize());
	}
	m_ShaderProgram->AssertPointersBound();
	// Don't use glMultiDrawElements here since it doesn't have a significant
	// performance impact and it suffers from various driver bugs (e.g. it breaks
	// in Mesa 7.10 swrast with index VBOs).
#if CONFIG2_GLES
	ENSURE(!m_Device->GetCapabilities().instancing);
	UNUSED2(indexCount);
	UNUSED2(firstIndex);
	UNUSED2(instanceCount);
#else
	glDrawElementsInstancedARB(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT,
		static_cast<const void*>((static_cast<const uint8_t*>(m_IndexBufferData) + sizeof(uint16_t) * firstIndex)),
		instanceCount);
#endif
	ogl_WarnIfError();
}

void CDeviceCommandContext::DrawIndexedInRange(
	const uint32_t firstIndex, const uint32_t indexCount,
	const uint32_t start, const uint32_t end)
{
	ENSURE(m_ShaderProgram);
	ENSURE(m_InsidePass);
	if (indexCount == 0)
		return;
	ENSURE(m_IndexBuffer || m_IndexBufferData);
	const void* indices =
		static_cast<const void*>((static_cast<const uint8_t*>(m_IndexBufferData) + sizeof(uint16_t) * firstIndex));
	m_ShaderProgram->AssertPointersBound();
	// Draw with DrawRangeElements where available, since it might be more
	// efficient for slow hardware.
#if CONFIG2_GLES
	UNUSED2(start);
	UNUSED2(end);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indices);
#else
	glDrawRangeElementsEXT(GL_TRIANGLES, start, end, indexCount, GL_UNSIGNED_SHORT, indices);
#endif
	ogl_WarnIfError();
}

void CDeviceCommandContext::BeginComputePass()
{
	ENSURE(!m_InsideFramebufferPass);
	ENSURE(!m_InsideComputePass);
	m_InsideComputePass = true;
}

void CDeviceCommandContext::EndComputePass()
{
	ENSURE(m_InsideComputePass);
	m_InsideComputePass = false;
}

void CDeviceCommandContext::Dispatch(
	const uint32_t groupCountX,
	const uint32_t groupCountY,
	const uint32_t groupCountZ)
{
#if !CONFIG2_GLES
	ENSURE(m_InsideComputePass);
	glDispatchCompute(groupCountX, groupCountY, groupCountZ);
	// Storage buffers should be managed explicitly by InsertMemoryBarrier.
	if (m_ShaderProgram->HasImageUniforms())
	{
		// TODO: we might want to do binding tracking to avoid redundant barriers.
		glMemoryBarrier(
			GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT );
	}
#else
	UNUSED2(groupCountX);
	UNUSED2(groupCountY);
	UNUSED2(groupCountZ);
#endif
}

void CDeviceCommandContext::InsertMemoryBarrier(
	const uint32_t UNUSED(srcStageMask), const uint32_t dstStageMask,
	const uint32_t srcAccessMask, const uint32_t dstAccessMask)
{
#if !CONFIG2_GLES
	ENSURE(!m_InsideFramebufferPass);
	GLbitfield barriers{0};
	if (srcAccessMask & Access::SHADER_WRITE)
	{
		if (dstStageMask & PipelineStage::VERTEX_INPUT)
		{
			if (dstAccessMask & Access::VERTEX_ATTRIBUTE_READ)
				barriers |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
			if (dstAccessMask & Access::INDEX_READ)
				barriers |= GL_ELEMENT_ARRAY_BARRIER_BIT;
		}
		if (dstStageMask & (PipelineStage::VERTEX_SHADER | PipelineStage::FRAGMENT_SHADER | PipelineStage::COMPUTE_SHADER))
		{
			if (dstAccessMask & (Access::SHADER_READ | Access::SHADER_WRITE))
				barriers |= GL_SHADER_STORAGE_BARRIER_BIT;
			if (dstAccessMask & Access::UNIFORM_READ)
				barriers |= GL_UNIFORM_BARRIER_BIT;
		}
	}
	if (barriers)
		glMemoryBarrier(barriers);
#else
	UNUSED2(dstStageMask);
	UNUSED2(srcAccessMask);
	UNUSED2(dstAccessMask);
#endif
}

void CDeviceCommandContext::SetTexture(const int32_t bindingSlot, ITexture* texture)
{
	ENSURE(m_ShaderProgram);
	ENSURE(texture);
	ENSURE(texture->GetUsage() & Renderer::Backend::ITexture::Usage::SAMPLED);

	const CShaderProgram::TextureUnit textureUnit =
		m_ShaderProgram->GetTextureUnit(bindingSlot);
	if (!textureUnit.type)
		return;

	if (textureUnit.type != GL_SAMPLER_2D &&
#if !CONFIG2_GLES
		textureUnit.type != GL_SAMPLER_2D_SHADOW &&
#endif
		textureUnit.type != GL_SAMPLER_CUBE)
	{
		LOGERROR("CDeviceCommandContext::SetTexture: expected sampler at binding slot");
		return;
	}

#if !CONFIG2_GLES
	if (textureUnit.type == GL_SAMPLER_2D_SHADOW)
	{
		if (!IsDepthFormat(texture->GetFormat()))
		{
			LOGERROR("CDeviceCommandContext::SetTexture: Invalid texture type (expected depth texture)");
			return;
		}
	}
#endif

	ENSURE(textureUnit.unit >= 0);
	const uint32_t unit = textureUnit.unit;
	if (unit >= m_BoundTextures.size())
	{
		LOGERROR("CDeviceCommandContext::SetTexture: Invalid texture unit (too big)");
		return;
	}
	BindTexture(unit, textureUnit.target, texture->As<CTexture>()->GetHandle());
}

void CDeviceCommandContext::SetStorageTexture(const int32_t bindingSlot, ITexture* texture)
{
#if !CONFIG2_GLES
	ENSURE(m_ShaderProgram);
	ENSURE(texture);
	ENSURE(texture->GetUsage() & Renderer::Backend::ITexture::Usage::STORAGE);

	const CShaderProgram::TextureUnit textureUnit =
		m_ShaderProgram->GetTextureUnit(bindingSlot);
	if (!textureUnit.type)
		return;
	ENSURE(textureUnit.type == GL_IMAGE_2D);
	ENSURE(texture->GetFormat() == Format::R8G8B8A8_UNORM);
	glBindImageTexture(textureUnit.unit, texture->As<CTexture>()->GetHandle(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
#else
	UNUSED2(bindingSlot);
	UNUSED2(texture);
#endif
}

void CDeviceCommandContext::SetStorageBuffer(const int32_t bindingSlot, IBuffer* buffer)
{
#if !CONFIG2_GLES
	if (bindingSlot < 0)
		return;
	ENSURE(m_ShaderProgram);
	ENSURE(buffer);
	ENSURE(buffer->GetUsage() & Renderer::Backend::IBuffer::Usage::STORAGE);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_ShaderProgram->GetStorageBuffer(bindingSlot), buffer->As<CBuffer>()->GetHandle());
#else
	UNUSED2(bindingSlot);
	UNUSED2(buffer);
#endif
}

void CDeviceCommandContext::SetUniform(
	const int32_t bindingSlot,
	const float value)
{
	ENSURE(m_ShaderProgram);
	m_ShaderProgram->SetUniform(bindingSlot, value);
}

void CDeviceCommandContext::SetUniform(
	const int32_t bindingSlot,
	const float valueX, const float valueY)
{
	ENSURE(m_ShaderProgram);
	m_ShaderProgram->SetUniform(bindingSlot, valueX, valueY);
}

void CDeviceCommandContext::SetUniform(
	const int32_t bindingSlot,
	const float valueX, const float valueY,
	const float valueZ)
{
	ENSURE(m_ShaderProgram);
	m_ShaderProgram->SetUniform(bindingSlot, valueX, valueY, valueZ);
}

void CDeviceCommandContext::SetUniform(
	const int32_t bindingSlot,
	const float valueX, const float valueY,
	const float valueZ, const float valueW)
{
	ENSURE(m_ShaderProgram);
	m_ShaderProgram->SetUniform(bindingSlot, valueX, valueY, valueZ, valueW);
}

void CDeviceCommandContext::SetUniform(
	const int32_t bindingSlot, PS::span<const float> values)
{
	ENSURE(m_ShaderProgram);
	m_ShaderProgram->SetUniform(bindingSlot, values);
}

CDeviceCommandContext::ScopedBind::ScopedBind(
	CDeviceCommandContext* deviceCommandContext,
	const GLenum target, const GLuint handle)
	: m_DeviceCommandContext(deviceCommandContext),
	m_OldBindUnit(deviceCommandContext->m_BoundTextures[deviceCommandContext->m_ActiveTextureUnit]),
	m_ActiveTextureUnit(deviceCommandContext->m_ActiveTextureUnit)
{
	const uint32_t unit = m_DeviceCommandContext->m_BoundTextures.size() - 1;
	m_DeviceCommandContext->BindTexture(unit, target, handle);
}

CDeviceCommandContext::ScopedBind::~ScopedBind()
{
	m_DeviceCommandContext->BindTexture(
		m_ActiveTextureUnit, m_OldBindUnit.target, m_OldBindUnit.handle);
}

CDeviceCommandContext::ScopedBufferBind::ScopedBufferBind(
	CDeviceCommandContext* deviceCommandContext, CBuffer* buffer)
	: m_DeviceCommandContext(deviceCommandContext)
{
	ENSURE(buffer);
	m_CacheIndex = static_cast<size_t>(buffer->GetType());
	ENSURE(m_CacheIndex < m_DeviceCommandContext->m_BoundBuffers.size());
	const GLenum target = BufferTypeToGLTarget(buffer->GetType());
	const GLuint handle = buffer->GetHandle();
	if (m_DeviceCommandContext->m_BoundBuffers[m_CacheIndex].first == target &&
		m_DeviceCommandContext->m_BoundBuffers[m_CacheIndex].second == handle)
	{
		// Use an invalid index as a sign that we don't need to restore the
		// bound buffer.
		m_CacheIndex = m_DeviceCommandContext->m_BoundBuffers.size();
	}
	else
	{
		glBindBufferARB(target, handle);
	}
}

CDeviceCommandContext::ScopedBufferBind::~ScopedBufferBind()
{
	if (m_CacheIndex >= m_DeviceCommandContext->m_BoundBuffers.size())
		return;
	glBindBufferARB(
		m_DeviceCommandContext->m_BoundBuffers[m_CacheIndex].first,
		m_DeviceCommandContext->m_BoundBuffers[m_CacheIndex].second);
}

} // namespace GL

} // namespace Backend

} // namespace Renderer
