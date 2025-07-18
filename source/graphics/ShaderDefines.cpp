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

#include "ShaderDefines.h"

#include "graphics/ShaderProgram.h"
#include "lib/hash.h"
#include "maths/Vector4D.h"
#include "ps/ThreadUtil.h"

#include <algorithm>
#include <sstream>

namespace std
{
template<>
struct hash<CVector4D>
{
	std::size_t operator()(const CVector4D& v) const
	{
		size_t hash = 0;
		hash_combine(hash, v.X);
		hash_combine(hash, v.Y);
		hash_combine(hash, v.Z);
		hash_combine(hash, v.W);
		return hash;
	}
};
}

bool operator==(const CShaderParams<CStrIntern>::SItems& a, const CShaderParams<CStrIntern>::SItems& b)
{
	return a.items == b.items;
}

bool operator==(const CShaderParams<CVector4D>::SItems& a, const CShaderParams<CVector4D>::SItems& b)
{
	return a.items == b.items;
}

template<typename value_t>
bool CShaderParams<value_t>::SItems::NameLess(const Item& a, const Item& b)
{
	return a.first < b.first;
}

template<typename value_t>
typename CShaderParams<value_t>::SItems* CShaderParams<value_t>::GetInterned(const SItems& items)
{
	ENSURE(Threading::IsMainThread()); // s_InternedItems is not thread-safe

	typename InternedItems_t::iterator it = s_InternedItems.find(items);
	if (it != s_InternedItems.end())
		return it->second.get();

	// Sanity test: the items list is meant to be sorted by name.
	// This is a reasonable place to verify that, since this will be called once per distinct SItems.
	ENSURE(std::is_sorted(items.items.begin(), items.items.end(), SItems::NameLess));

	std::shared_ptr<SItems> ptr = std::make_shared<SItems>(items);
	s_InternedItems.insert(std::make_pair(items, ptr));
	return ptr.get();
}

template<typename value_t>
CShaderParams<value_t>::CShaderParams()
{
	*this = s_Empty;
}

template<typename value_t>
CShaderParams<value_t>::CShaderParams(SItems* items) : m_Items(items)
{
}

template<typename value_t>
CShaderParams<value_t> CShaderParams<value_t>::CreateEmpty()
{
	SItems items;
	items.RecalcHash();
	return CShaderParams(GetInterned(items));
}

template<typename value_t>
void CShaderParams<value_t>::Set(CStrIntern name, const value_t& value)
{
	SItems items = *m_Items;

	typename SItems::Item addedItem = std::make_pair(name, value);

	// Add the new item in a way that preserves the sortedness and uniqueness of item names
	for (typename SItems::ItemsContainers::iterator it = items.items.begin(); ; ++it)
	{
		if (it == items.items.end() || addedItem.first < it->first)
		{
			items.items.insert(it, addedItem);
			break;
		}
		else if (addedItem.first == it->first)
		{
			it->second = addedItem.second;
			break;
		}
	}

	items.RecalcHash();
	m_Items = GetInterned(items);
}

template<typename value_t>
void CShaderParams<value_t>::SetMany(const CShaderParams& params)
{
	SItems items;
	// set_union merges the two sorted lists into a new sorted list;
	// if two items are equivalent (i.e. equal names, possibly different values)
	// then the one from the first list is kept
	std::set_union(
		params.m_Items->items.begin(), params.m_Items->items.end(),
		m_Items->items.begin(), m_Items->items.end(),
		std::inserter(items.items, items.items.begin()),
		SItems::NameLess);
	items.RecalcHash();
	m_Items = GetInterned(items);
}

template<typename value_t>
std::map<CStrIntern, value_t> CShaderParams<value_t>::GetMap() const
{
	std::map<CStrIntern, value_t> ret;
	for (const typename SItems::Item& item : m_Items->items)
		ret[item.first] = item.second;
	return ret;
}

template<typename value_t>
size_t CShaderParams<value_t>::GetHash() const
{
	return m_Items->hash;
}

template<typename value_t>
void CShaderParams<value_t>::SItems::RecalcHash()
{
	size_t h = 0;
	for (const Item& item : items)
	{
		hash_combine(h, item.first);
		hash_combine(h, item.second);
	}
	hash = h;
}


void CShaderDefines::Add(CStrIntern name, CStrIntern value)
{
	Set(name, value);
}

int CShaderDefines::GetInt(const char* name) const
{
	CStrIntern nameIntern(name);
	for (const SItems::Item& item : m_Items->items)
	{
		if (item.first == nameIntern)
		{
			int ret;
			std::stringstream str(item.second.c_str());
			str >> ret;
			return ret;
		}
	}
	return 0;
}


void CShaderUniforms::Add(const char* name, const CVector4D& value)
{
	Set(CStrIntern(name), value);
}

CVector4D CShaderUniforms::GetVector(const char* name) const
{
	CStrIntern nameIntern(name);
	for (const SItems::Item& item : m_Items->items)
	{
		if (item.first == nameIntern)
		{
			return item.second;
		}
	}
	return CVector4D();
}

void CShaderUniforms::BindUniforms(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext,
	Renderer::Backend::IShaderProgram* shader) const
{
	for (const SItems::Item& item : m_Items->items)
	{
		const CVector4D& v = item.second;
		deviceCommandContext->SetUniform(
			shader->GetBindingSlot(item.first), v.AsFloatArray());
	}
}

void CShaderRenderQueries::Add(const char* name)
{
	if (name == CStr("sim_time"))
	{
		m_Items.emplace_back(RQUERY_TIME, CStrIntern(name));
	}
	else if (name == CStr("water_tex"))
	{
		m_Items.emplace_back(RQUERY_WATER_TEX, CStrIntern(name));
	}
	else if (name == CStr("sky_cube"))
	{
		m_Items.emplace_back(RQUERY_SKY_CUBE, CStrIntern(name));
	}
}

// Explicit instantiations:

template<> CShaderParams<CStrIntern>::InternedItems_t CShaderParams<CStrIntern>::s_InternedItems = CShaderParams<CStrIntern>::InternedItems_t();
template<> CShaderParams<CVector4D>::InternedItems_t CShaderParams<CVector4D>::s_InternedItems = CShaderParams<CVector4D>::InternedItems_t();

template<> CShaderParams<CStrIntern> CShaderParams<CStrIntern>::s_Empty = CShaderParams<CStrIntern>::CreateEmpty();
template<> CShaderParams<CVector4D> CShaderParams<CVector4D>::s_Empty = CShaderParams<CVector4D>::CreateEmpty();

template class CShaderParams<CStrIntern>;
template class CShaderParams<CVector4D>;
