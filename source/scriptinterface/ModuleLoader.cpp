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

#include "ModuleLoader.h"

#include "ps/CStr.h"
#include "ps/Filesystem.h"
#include "scriptinterface/Object.h"
#include "scriptinterface/ScriptConversions.h"
#include "scriptinterface/ScriptInterface.h"

#include "js/Modules.h"
#include <fmt/format.h>
#include <stdexcept>

namespace Script
{
namespace
{
[[nodiscard]] std::string GetCode(const VfsPath& filePath)
{
	if (!VfsFileExists(filePath))
		throw std::runtime_error{fmt::format("The file \"{}\" does not exist.", filePath.string8())};

	if (filePath.Extension().string8() != ".js")
	{
		throw std::runtime_error{fmt::format("The file \"{}\" is not a JavaScript module.",
			filePath.string8())};
	}

	CVFSFile file;
	const PSRETURN ret{file.Load(g_VFS, filePath)};
	if (ret != PSRETURN_OK)
	{
		throw std::runtime_error{fmt::format("Failed to load file \"{}\": {}.", filePath.string8(),
			GetErrorString(ret))};
	}

	return file.DecodeUTF8();
}

[[nodiscard]] JSObject* CompileModule(const ScriptRequest& rq, ModuleLoader::RegistryType& registry,
	const VfsPath& filePath)
{
	const VfsPath normalizedPath{filePath.fileSystemPath().lexically_normal().generic_string()};
	const auto insertResult = registry.try_emplace(normalizedPath, rq, normalizedPath);
	return std::get<1>(*std::get<0>(insertResult)).m_ModuleObject;
}

void Evaluate(const ScriptRequest& rq, JS::HandleObject mod)
{
	if (!JS::ModuleLink(rq.cx, mod))
	{
		ScriptException::CatchPending(rq);
		throw std::invalid_argument{"Unable to link module."};
	}

	JS::RootedValue val{rq.cx};
	if (!JS::ModuleEvaluate(rq.cx, mod, &val))
	{
		ScriptException::CatchPending(rq);
		throw std::invalid_argument{"Unable to evaluate module."};
	}
}
} // anonymous namespace

ModuleLoader::CompiledModule::CompiledModule(const ScriptRequest& rq, const VfsPath& filePath):
	m_ModuleObject(rq.cx)
{
	const std::string code{GetCode(filePath)};

	JS::CompileOptions options{rq.cx};
	const std::string filePathStr{filePath.string8()};
	options.setFileAndLine(filePathStr.c_str(), 1);

	JS::SourceText<mozilla::Utf8Unit> src;
	if (!src.init(rq.cx, code.c_str(), code.length(), JS::SourceOwnership::Borrowed))
		throw std::invalid_argument{fmt::format("Unable to read code file: \"{}\".", filePathStr)};

	m_ModuleObject = JS::CompileModule(rq.cx, options, src);

	if (!m_ModuleObject)
	{
		ScriptException::CatchPending(rq);
		throw std::invalid_argument{fmt::format("Unable to compile module: \"{}\".",
			filePathStr)};
	}
}

void ModuleLoader::LoadModule(const ScriptRequest& rq, const VfsPath& modulePath)
{
	JS::RootedObject mod{rq.cx, CompileModule(rq, m_Registry, modulePath)};
	Evaluate(rq, mod);
}

[[nodiscard]] JSObject* ModuleLoader::ResolveHook(JSContext* cx, JS::HandleValue,
	JS::HandleObject moduleRequest) noexcept
{
	try
	{
		const ScriptRequest rq{cx};
		std::string includeString;
		const JS::RootedValue pathValue{rq.cx,
			JS::StringValue(JS::GetModuleRequestSpecifier(rq.cx, moduleRequest))};
		if (!Script::FromJSVal(rq, pathValue, includeString))
			throw std::logic_error{"The module-name to import isn't a string."};

		return CompileModule(rq, rq.GetScriptInterface().GetModuleLoader().m_Registry, includeString);
	}
	catch (const std::exception& e)
	{
		LOGERROR("%s", e.what());
		return nullptr;
	}
	catch (...)
	{
		LOGERROR("Error compiling module.");
		return nullptr;
	}
}
} // namespace Script
