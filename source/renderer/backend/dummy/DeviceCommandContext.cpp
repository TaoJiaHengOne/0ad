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

#include "renderer/backend/dummy/Buffer.h"
#include "renderer/backend/dummy/Device.h"
#include "renderer/backend/dummy/Framebuffer.h"
#include "renderer/backend/dummy/ShaderProgram.h"
#include "renderer/backend/dummy/Texture.h"

namespace Renderer
{

namespace Backend
{

namespace Dummy
{

// static
std::unique_ptr<CDeviceCommandContext> CDeviceCommandContext::Create(CDevice* device)
{
	std::unique_ptr<CDeviceCommandContext> deviceCommandContext(new CDeviceCommandContext());
	deviceCommandContext->m_Device = device;
	return deviceCommandContext;
}

CDeviceCommandContext::CDeviceCommandContext() = default;

CDeviceCommandContext::~CDeviceCommandContext() = default;

IDevice* CDeviceCommandContext::GetDevice()
{
	return m_Device;
}

void CDeviceCommandContext::SetGraphicsPipelineState(
	IGraphicsPipelineState*)
{
}

void CDeviceCommandContext::SetComputePipelineState(
	IComputePipelineState*)
{
}

void CDeviceCommandContext::UploadTexture(
	ITexture*, const Format, const void*, const size_t,
	const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::UploadTextureRegion(
	ITexture*, const Format, const void*, const size_t,
	const uint32_t, const uint32_t,	const uint32_t, const uint32_t,
	const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::UploadBuffer(IBuffer*, const void*, const uint32_t)
{
}

void CDeviceCommandContext::UploadBuffer(IBuffer*, const UploadBufferFunction&)
{
}

void CDeviceCommandContext::UploadBufferRegion(
	IBuffer*, const void*, const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::UploadBufferRegion(
	IBuffer*, const uint32_t, const uint32_t, const UploadBufferFunction&)
{
}

void CDeviceCommandContext::InsertTimestampQuery(const uint32_t, const bool)
{
}

void CDeviceCommandContext::BeginScopedLabel(const char*)
{
}

void CDeviceCommandContext::EndScopedLabel()
{
}

void CDeviceCommandContext::Flush()
{
}

void CDeviceCommandContext::BlitFramebuffer(
	IFramebuffer*, IFramebuffer*, const Rect&, const Rect&, const Sampler::Filter)
{
}

void CDeviceCommandContext::ResolveFramebuffer(IFramebuffer*, IFramebuffer*)
{
}

void CDeviceCommandContext::ClearFramebuffer(const bool, const bool, const bool)
{
}

void CDeviceCommandContext::BeginFramebufferPass(IFramebuffer*)
{
}

void CDeviceCommandContext::EndFramebufferPass()
{
}

void CDeviceCommandContext::ReadbackFramebufferSync(
	const uint32_t, const uint32_t, const uint32_t, const uint32_t, void*)
{
}

void CDeviceCommandContext::SetScissors(const uint32_t, const Rect*)
{
}

void CDeviceCommandContext::SetViewports(const uint32_t, const Rect*)
{
}

void CDeviceCommandContext::SetVertexInputLayout(IVertexInputLayout*)
{
}

void CDeviceCommandContext::SetVertexBuffer(const uint32_t, IBuffer*, const uint32_t)
{
}

void CDeviceCommandContext::SetVertexBufferData(
	const uint32_t, const void*, const uint32_t)
{
}

void CDeviceCommandContext::SetIndexBuffer(IBuffer*)
{
}

void CDeviceCommandContext::SetIndexBufferData(const void*, const uint32_t)
{
}

void CDeviceCommandContext::BeginPass()
{
}

void CDeviceCommandContext::EndPass()
{
}

void CDeviceCommandContext::Draw(const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::DrawIndexed(const uint32_t, const uint32_t, const int32_t)
{
}

void CDeviceCommandContext::DrawInstanced(
	const uint32_t, const uint32_t, const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::DrawIndexedInstanced(
	const uint32_t, const uint32_t, const uint32_t, const uint32_t, const int32_t)
{
}

void CDeviceCommandContext::DrawIndexedInRange(
	const uint32_t, const uint32_t, const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::BeginComputePass()
{
}

void CDeviceCommandContext::EndComputePass()
{
}

void CDeviceCommandContext::Dispatch(const uint32_t, const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::InsertMemoryBarrier(
	const uint32_t, const uint32_t, const uint32_t, const uint32_t)
{
}

void CDeviceCommandContext::SetTexture(const int32_t, ITexture*)
{
}

void CDeviceCommandContext::SetStorageTexture(const int32_t, ITexture*)
{
}

void CDeviceCommandContext::SetStorageBuffer(const int32_t, IBuffer*)
{
}

void CDeviceCommandContext::SetUniform(const int32_t, const float)
{
}

void CDeviceCommandContext::SetUniform(const int32_t, const float, const float)
{
}

void CDeviceCommandContext::SetUniform(
	const int32_t, const float, const float, const float)
{
}

void CDeviceCommandContext::SetUniform(
	const int32_t, const float, const float, const float, const float)
{
}

void CDeviceCommandContext::SetUniform(const int32_t, PS::span<const float>)
{
}

} // namespace Dummy

} // namespace Backend

} // namespace Renderer
