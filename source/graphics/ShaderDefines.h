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

#ifndef INCLUDED_SHADERDEFINES
#define INCLUDED_SHADERDEFINES

#include "ps/containers/StaticVector.h"
#include "ps/CStr.h"
#include "ps/CStrIntern.h"
#include "renderer/backend/IDeviceCommandContext.h"
#include "renderer/backend/IShaderProgram.h"

#include <map>
#include <unordered_map>
#include <vector>

class CVector4D;

/**
 * Represents a mapping of name strings to value, for use with
 * CShaderDefines (values are strings) and CShaderUniforms (values are vec4s).
 *
 * Stored as interned vectors of name-value pairs, to support high performance
 * comparison operators.
 *
 * Not thread-safe - must only be used from the main thread.
 */
template<typename value_t>
class CShaderParams
{
public:
	/**
	 * Create an empty map of defines.
	 */
	CShaderParams();

	/**
	 * Add a name and associated value to the map of parameters.
	 * If the name is already defined, its value will be replaced.
	 */
	void Set(CStrIntern name, const value_t& value);

	/**
	 * Add all the names and values from another set of parameters.
	 * If any name is already defined in this object, its value will be replaced.
	 */
	void SetMany(const CShaderParams& params);

	/**
	 * Return a copy of the current name/value mapping.
	 */
	std::map<CStrIntern, value_t> GetMap() const;

	/**
	 * Return a hash of the current mapping.
	 */
	size_t GetHash() const;

	/**
	 * Compare with some arbitrary total order.
	 * The order may be different each time the application is run
	 * (it is based on interned memory addresses).
	 */
	bool operator<(const CShaderParams& b) const
	{
		return m_Items < b.m_Items;
	}

	/**
	 * Fast equality comparison.
	 */
	bool operator==(const CShaderParams& b) const
	{
		return m_Items == b.m_Items;
	}

	/**
	 * Fast inequality comparison.
	 */
	bool operator!=(const CShaderParams& b) const
	{
		return m_Items != b.m_Items;
	}

	struct SItems
	{
		// Name/value pair
		using Item = std::pair<CStrIntern, value_t>;

		// Sorted by name; no duplicated names. We can use the StaticVector
		// because we shouldn't have too many shader parameters of a single
		// type.
		using ItemsContainers = PS::StaticVector<Item, 32>;
		ItemsContainers items;

		size_t hash;

		void RecalcHash();

		static bool NameLess(const Item& a, const Item& b);
	};

	struct SItemsHash
	{
		std::size_t operator()(const SItems& items) const
		{
			return items.hash;
		}
	};

protected:
 	SItems* m_Items; // interned value

private:
	using InternedItems_t = std::unordered_map<SItems, std::shared_ptr<SItems>, SItemsHash>;
	static InternedItems_t s_InternedItems;

	/**
	 * Returns a pointer to an SItems equal to @p items.
	 * The pointer will be valid forever, and the same pointer will be returned
	 * for any subsequent requests for an equal items list.
	 */
	static SItems* GetInterned(const SItems& items);

	CShaderParams(SItems* items);
	static CShaderParams CreateEmpty();
	static CShaderParams s_Empty;
};

/**
 * Represents a mapping of name strings to value strings, for use with
 * \#if and \#ifdef and similar conditionals in shaders.
 *
 * Not thread-safe - must only be used from the main thread.
 */
class CShaderDefines : public CShaderParams<CStrIntern>
{
public:
	/**
	 * Add a name and associated value to the map of defines.
	 * If the name is already defined, its value will be replaced.
	 */
	void Add(CStrIntern name, CStrIntern value);

	/**
	 * Return the value for the given name as an integer, or 0 if not defined.
	 */
	int GetInt(const char* name) const;
};

/**
 * Represents a mapping of name strings to value CVector4Ds, for use with
 * uniforms in shaders.
 *
 * Not thread-safe - must only be used from the main thread.
 */
class CShaderUniforms : public CShaderParams<CVector4D>
{
public:
	/**
	 * Add a name and associated value to the map of uniforms.
	 * If the name is already defined, its value will be replaced.
	 */
	void Add(const char* name, const CVector4D& value);

	/**
	 * Return the value for the given name, or (0,0,0,0) if not defined.
	 */
	CVector4D GetVector(const char* name) const;

	/**
	 * Bind the collection of uniforms onto the given shader.
	 */
	void BindUniforms(
		Renderer::Backend::IDeviceCommandContext* deviceCommandContext,
		Renderer::Backend::IShaderProgram* shader) const;
};

// Add here the types of queries we can make in the renderer
enum RENDER_QUERIES
{
	RQUERY_TIME,
	RQUERY_WATER_TEX,
	RQUERY_SKY_CUBE
};

/**
 * Uniform values that need to be evaluated in the renderer.
 *
 * Not thread-safe - must only be used from the main thread.
 */
class CShaderRenderQueries
{
public:
	using RenderQuery = std::pair<int, CStrIntern>;

	void Add(const char* name);
	size_t GetSize() const { return m_Items.size(); }
	RenderQuery GetItem(size_t i) const { return m_Items[i]; }
private:
	std::vector<RenderQuery> m_Items;
};

#endif // INCLUDED_SHADERDEFINES
