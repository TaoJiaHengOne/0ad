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
#include "scriptinterface/FunctionWrapper.h"
#include "scriptinterface/Object.h"
#include "scriptinterface/Promises.h"
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

[[nodiscard]] JSObject* Evaluate(const ScriptRequest& rq, JS::HandleObject mod)
{
	if (!JS::ModuleLink(rq.cx, mod))
	{
		ScriptException::CatchPending(rq);
		throw std::invalid_argument{"Unable to link module."};
	}

	JS::RootedValue val{rq.cx};
	if (!JS::ModuleEvaluate(rq.cx, mod, &val) || !val.isObject())
	{
		ScriptException::CatchPending(rq);
		throw std::invalid_argument{"Unable to evaluate module."};
	}

	return &val.toObject();
}

template<bool reject>
bool Call(JSContext* cx, const unsigned argc, JS::Value* vp)
{
	JS::CallArgs args{JS::CallArgsFromVp(argc, vp)};
	const ScriptRequest rq{cx};

	const auto statusPtr{JS::GetMaybePtrFromReservedSlot<ModuleLoader::Future::Status>(
		&args.callee(), 0)};
	if (!statusPtr)
		return true;

	auto& status = *statusPtr;

	if (reject)
	{
		JS::HandleValue error{args.get(0)};
		std::string asString;
		ScriptFunction::Call(rq, error, "toString", asString);
		std::string stack;
		Script::GetProperty(rq, error, "stack", stack);
		status = ModuleLoader::Future::Rejected{std::make_exception_ptr(std::runtime_error{
			asString + '\n' + stack})};
		return true;
	}

	const auto evaluatingStatus{std::get_if<ModuleLoader::Future::Evaluating>(&status)};
	if (!evaluatingStatus)
	{
		status = ModuleLoader::Future::Rejected{std::make_exception_ptr(std::runtime_error{
			"Future is not Pending."})};
		return true;
	}
	status = ModuleLoader::Future::Fulfilled{evaluatingStatus->moduleNamespace};
	return true;
}

template<bool reject>
constexpr JSClassOps callbackClassOps{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	/*call =*/Call<reject>, nullptr, nullptr};

template<bool reject>
constexpr JSClass callbackClass{"Callback", JSCLASS_HAS_RESERVED_SLOTS(1), &callbackClassOps<reject>};
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

ModuleLoader::Future::Future(const ScriptRequest& rq, ModuleLoader& loader, const VfsPath& modulePath):
	m_Status{Evaluating{{rq.cx, nullptr}, {rq.cx, JS_NewObject(rq.cx, &callbackClass<false>)},
		{rq.cx, JS_NewObject(rq.cx, &callbackClass<true>)}}}
{
	// It's possible to access exported values before the complete module is evaluated (whenever
	// something is `export`-ed before a top-level `await`).
	// Those "partial" module namespaces are not exposed for the following reasons:
	// - The use case for them is too limited.
	// - JS developers are used to getting either a complete namespace or nothing.
	// - Accessing values which are not yet exported results in an error. These errors might implicitly be
	//	dropped.

	JS::RootedObject mod{rq.cx, CompileModule(rq, loader.m_Registry, modulePath)};
	JS::RootedObject promise{rq.cx, Evaluate(rq, mod)};
	Evaluating& evaluatingStatus{std::get<Evaluating>(m_Status)};
	evaluatingStatus.moduleNamespace = JS::GetModuleNamespace(rq.cx, mod);

	SetReservedSlot(JS::PrivateValue(static_cast<void*>(&m_Status)));

	if (!JS::AddPromiseReactions(rq.cx, promise, evaluatingStatus.fulfill, evaluatingStatus.reject))
		throw std::runtime_error{"Failed adding promise reaction."};
}

ModuleLoader::Future::Future(Future&& other) noexcept:
	m_Status{std::exchange(other.m_Status, Invalid{})}
{
	SetReservedSlot(JS::PrivateValue(static_cast<void*>(&m_Status)));
}

ModuleLoader::Future& ModuleLoader::Future::operator=(Future&& other) noexcept
{
	SetReservedSlot(JS::UndefinedValue());
	m_Status = std::exchange(other.m_Status, Invalid{});
	SetReservedSlot(JS::PrivateValue(static_cast<void*>(&m_Status)));

	return *this;
}

ModuleLoader::Future::~Future()
{
	SetReservedSlot(JS::UndefinedValue());
}

[[nodiscard]] bool ModuleLoader::Future::IsDone() const noexcept
{
	return std::holds_alternative<Fulfilled>(m_Status) || std::holds_alternative<Rejected>(m_Status);
}

[[nodiscard]] JSObject* ModuleLoader::Future::Get()
{
	if (std::holds_alternative<Fulfilled>(m_Status))
		return std::get<Fulfilled>(std::exchange(m_Status, Invalid{})).moduleNamespace;
	std::exception_ptr error{std::move(std::get<Rejected>(m_Status).error)};
	m_Status = Invalid{};
	std::rethrow_exception(std::move(error));
}

void ModuleLoader::Future::SetReservedSlot(JS::Value privateValue) noexcept
{
	Evaluating* evaluatingStatus{std::get_if<Evaluating>(&m_Status)};
	if (!evaluatingStatus)
		return;
	if (evaluatingStatus->fulfill)
		JS::SetReservedSlot(evaluatingStatus->fulfill, 0, privateValue);
	if (evaluatingStatus->reject)
		JS::SetReservedSlot(evaluatingStatus->reject, 0, privateValue);
}

[[nodiscard]] ModuleLoader::Future ModuleLoader::LoadModule(const ScriptRequest& rq,
	const VfsPath& modulePath)
{
	return Future{rq, *this, modulePath};
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
