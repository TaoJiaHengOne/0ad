/* Copyright (C) 2024 Wildfire Games.
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

#ifndef INCLUDED_RENDERER_BACKEND_VULKAN_DESCRIPTORMANAGER
#define INCLUDED_RENDERER_BACKEND_VULKAN_DESCRIPTORMANAGER

#include "ps/CStrIntern.h"
#include "renderer/backend/Sampler.h"
#include "renderer/backend/vulkan/Buffer.h"
#include "renderer/backend/vulkan/Device.h"
#include "renderer/backend/vulkan/Texture.h"

#include <glad/vulkan.h>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Renderer
{

namespace Backend
{

namespace Vulkan
{

class CDevice;

class CDescriptorManager
{
public:
	CDescriptorManager(CDevice* device, const bool useDescriptorIndexing);
	~CDescriptorManager();

	bool UseDescriptorIndexing() const { return m_UseDescriptorIndexing; }

	/**
	 * @return a single type descriptor set layout with the number of bindings
	 * equals to the size. The returned layout is owned by the manager.
	 */
	VkDescriptorSetLayout GetSingleTypeDescritorSetLayout(
		VkDescriptorType type, const uint32_t size);

	VkDescriptorSet GetSingleTypeDescritorSet(
		VkDescriptorType type, VkDescriptorSetLayout layout,
		const std::vector<DeviceObjectUID>& texturesUID,
		const std::vector<CTexture*>& textures);

	VkDescriptorSet GetSingleTypeDescritorSet(
		VkDescriptorType type, VkDescriptorSetLayout layout,
		const std::vector<DeviceObjectUID>& buffersUID,
		const std::vector<CBuffer*>& buffers);

	uint32_t GetUniformSet() const;

	uint32_t GetTextureDescriptor(CTexture* texture);

	void OnTextureDestroy(const DeviceObjectUID uid);

	void OnBufferDestroy(const DeviceObjectUID uid);

	const VkDescriptorSetLayout& GetDescriptorIndexingSetLayout() const { return m_DescriptorIndexingSetLayout; }
	const VkDescriptorSetLayout& GetUniformDescriptorSetLayout() const { return m_UniformDescriptorSetLayout; }
	const VkDescriptorSet& GetDescriptorIndexingSet() { return m_DescriptorIndexingSet; }

	const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }

private:
	struct SingleTypePool
	{
		VkDescriptorSetLayout layout;
		VkDescriptorPool pool;
		int16_t firstFreeIndex = 0;
		static constexpr int16_t INVALID_INDEX = -1;
		struct Element
		{
			VkDescriptorSet set = VK_NULL_HANDLE;
			uint32_t version = 0;
			int16_t nextFreeIndex = INVALID_INDEX;
		};
		std::vector<Element> elements;
	};
	SingleTypePool& GetSingleTypePool(const VkDescriptorType type, const uint32_t size);

	std::pair<VkDescriptorSet, bool> GetSingleTypeDescritorSetImpl(
		VkDescriptorType type, VkDescriptorSetLayout layout,
		const std::vector<DeviceObjectUID>& uids);

	void OnDeviceObjectDestroy(const DeviceObjectUID uid);

	CDevice* m_Device = nullptr;

	bool m_UseDescriptorIndexing = false;

	VkDescriptorPool m_DescriptorIndexingPool = VK_NULL_HANDLE;
	VkDescriptorSet m_DescriptorIndexingSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_DescriptorIndexingSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_UniformDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;

	static constexpr uint32_t DESCRIPTOR_INDEXING_BINDING_SIZE = 16384;
	static constexpr uint32_t NUMBER_OF_BINDINGS_PER_DESCRIPTOR_INDEXING_SET = 3;

	struct DescriptorIndexingBindingMap
	{
		static_assert(std::numeric_limits<int16_t>::max() >= DESCRIPTOR_INDEXING_BINDING_SIZE);
		int16_t firstFreeIndex = 0;
		std::vector<int16_t> elements;
		std::unordered_map<DeviceObjectUID, int16_t> map;
	};
	std::array<DescriptorIndexingBindingMap, NUMBER_OF_BINDINGS_PER_DESCRIPTOR_INDEXING_SET>
		m_DescriptorIndexingBindings;
	std::unordered_map<DeviceObjectUID, uint32_t> m_TextureToBindingMap;

	std::unordered_map<VkDescriptorType, std::vector<SingleTypePool>> m_SingleTypePools;
	struct SingleTypePoolReference
	{
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		uint32_t version = 0;
		int16_t elementIndex = SingleTypePool::INVALID_INDEX;
		uint8_t size = 0;
	};
	std::unordered_map<DeviceObjectUID, std::vector<SingleTypePoolReference>> m_UIDToSingleTypePoolMap;

	using SingleTypeCacheKey = std::pair<VkDescriptorSetLayout, std::vector<DeviceObjectUID>>;
	struct SingleTypeCacheKeyHash
	{
		size_t operator()(const SingleTypeCacheKey& key) const;
	};
	std::unordered_map<SingleTypeCacheKey, VkDescriptorSet, SingleTypeCacheKeyHash> m_SingleTypeSets;

	std::unique_ptr<ITexture> m_ErrorTexture;
};

// TODO: ideally we might want to separate a set and its mapping.
template<typename DeviceObject>
class CSingleTypeDescriptorSetBinding
{
public:
	CSingleTypeDescriptorSetBinding(CDevice* device, const VkDescriptorType type,
		const uint32_t size, std::unordered_map<CStrIntern, uint32_t> mapping)
		: m_Device{device}, m_Type{type}, m_Mapping{std::move(mapping)}
	{
		m_BoundDeviceObjects.resize(size);
		m_BoundUIDs.resize(size);
		m_DescriptorSetLayout =
			m_Device->GetDescriptorManager().GetSingleTypeDescritorSetLayout(m_Type, size);
	}

	int32_t GetBindingSlot(const CStrIntern name) const
	{
		const auto it = m_Mapping.find(name);
		return it != m_Mapping.end() ? it->second : -1;
	}

	void SetObject(const int32_t bindingSlot, DeviceObject* object)
	{
		if (m_BoundUIDs[bindingSlot] == object->GetUID())
			return;
		m_BoundUIDs[bindingSlot] = object->GetUID();
		m_BoundDeviceObjects[bindingSlot] = object;
		m_Outdated = true;
	}

	bool IsOutdated() const { return m_Outdated; }

	VkDescriptorSet UpdateAndReturnDescriptorSet()
	{
		ENSURE(m_Outdated);
		m_Outdated = false;

		VkDescriptorSet descriptorSet =
			m_Device->GetDescriptorManager().GetSingleTypeDescritorSet(
				m_Type, m_DescriptorSetLayout, m_BoundUIDs, m_BoundDeviceObjects);
		ENSURE(descriptorSet != VK_NULL_HANDLE);

		return descriptorSet;
	}

	void Unbind()
	{
		std::fill(m_BoundDeviceObjects.begin(), m_BoundDeviceObjects.end(), nullptr);
		std::fill(m_BoundUIDs.begin(), m_BoundUIDs.end(), INVALID_DEVICE_OBJECT_UID);
		m_Outdated = true;
	}

	VkDescriptorSetLayout GetDescriptorSetLayout() { return m_DescriptorSetLayout; }

	const std::vector<DeviceObject*>& GetBoundDeviceObjects() const { return m_BoundDeviceObjects; }

private:
	CDevice* const m_Device;
	const VkDescriptorType m_Type;
	const std::unordered_map<CStrIntern, uint32_t> m_Mapping;

	bool m_Outdated{true};

	VkDescriptorSetLayout m_DescriptorSetLayout{VK_NULL_HANDLE};

	std::vector<DeviceObject*> m_BoundDeviceObjects;
	std::vector<DeviceObjectUID> m_BoundUIDs;
};

} // namespace Vulkan

} // namespace Backend

} // namespace Renderer

#endif // INCLUDED_RENDERER_BACKEND_VULKAN_DESCRIPTORMANAGER
