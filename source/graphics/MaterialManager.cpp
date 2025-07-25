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

#include "MaterialManager.h"

#include "graphics/PreprocessorWrapper.h"
#include "maths/MathUtil.h"
#include "maths/Vector4D.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/CStrInternStatic.h"
#include "ps/Filesystem.h"
#include "ps/XML/Xeromyces.h"
#include "renderer/RenderingOptions.h"

#include <sstream>

CMaterialManager::CMaterialManager() :
	qualityLevel{Clamp(g_ConfigDB.Get("materialmgr.quality", 5.0f), 0.0f, 10.0f)}
{
	if (VfsDirectoryExists(L"art/materials/") &&
		!g_Xeromyces.AddValidator(g_VFS, "material", "art/materials/material.rng"))
	{
		LOGERROR("CMaterialManager: failed to load grammar file 'art/materials/material.rng'");
	}
}

CMaterial CMaterialManager::LoadMaterial(const VfsPath& pathname)
{
	if (pathname.empty())
		return CMaterial();

	std::map<VfsPath, CMaterial>::iterator iter = m_Materials.find(pathname);
	if (iter != m_Materials.end())
		return iter->second;

	CXeromyces xeroFile;
	if (xeroFile.Load(g_VFS, pathname, "material") != PSRETURN_OK)
		return CMaterial();

	#define EL(x) int el_##x = xeroFile.GetElementID(#x)
	#define AT(x) int at_##x = xeroFile.GetAttributeID(#x)
	EL(alpha_blending);
	EL(alternative);
	EL(define);
	EL(shader);
	EL(uniform);
	EL(renderquery);
	EL(required_texture);
	AT(effect);
	AT(if);
	AT(define);
	AT(quality);
	AT(material);
	AT(name);
	AT(value);
	#undef AT
	#undef EL

	CPreprocessorWrapper preprocessor;
	CMaterial material;
	material.AddStaticUniform("qualityLevel", CVector4D(qualityLevel, 0, 0, 0));

	XMBElement root = xeroFile.GetRoot();
	XERO_ITER_EL(root, node)
	{
		int token = node.GetNodeName();
		XMBAttributeList attrs = node.GetAttributes();
		if (token == el_alternative)
		{
			CStr cond = attrs.GetNamedItem(at_if);
			if (cond.empty() || !preprocessor.TestConditional(cond))
			{
				cond = attrs.GetNamedItem(at_quality);
				if (cond.empty())
					continue;
				else
				{
					if (cond.ToFloat() <= qualityLevel)
						continue;
				}
			}

			material = LoadMaterial(VfsPath("art/materials") / attrs.GetNamedItem(at_material).FromUTF8());
			break;
		}
		else if (token == el_alpha_blending)
		{
			material.SetUsesAlphaBlending(true);
		}
		else if (token == el_shader)
		{
			material.SetShaderEffect(attrs.GetNamedItem(at_effect));
		}
		else if (token == el_define)
		{
			material.AddShaderDefine(CStrIntern(attrs.GetNamedItem(at_name)), CStrIntern(attrs.GetNamedItem(at_value)));
		}
		else if (token == el_uniform)
		{
			std::stringstream str(attrs.GetNamedItem(at_value));
			CVector4D vec;
			str >> vec.X >> vec.Y >> vec.Z >> vec.W;
			material.AddStaticUniform(attrs.GetNamedItem(at_name).c_str(), vec);
		}
		else if (token == el_renderquery)
		{
			material.AddRenderQuery(attrs.GetNamedItem(at_name).c_str());
		}
		else if (token == el_required_texture)
		{
			material.AddRequiredSampler(attrs.GetNamedItem(at_name));
			if (!attrs.GetNamedItem(at_define).empty())
				material.AddShaderDefine(CStrIntern(attrs.GetNamedItem(at_define)), str_1);
		}
	}

	m_Materials[pathname] = material;
	return material;
}
