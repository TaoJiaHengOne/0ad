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

#ifndef INCLUDED_SCRIPTMODULELOADER
#define INCLUDED_SCRIPTMODULELOADER

#include "lib/file/vfs/vfs_path.h"
#include "scriptinterface/ScriptTypes.h"

#include <exception>
#include <unordered_map>
#include <variant>

class ScriptContext;
class ScriptRequest;

namespace Script
{
class ModuleLoader
{
public:
	friend ScriptContext;

	class CompiledModule
	{
	public:
		CompiledModule(const ScriptRequest& rq, const VfsPath& filePath);
		JS::PersistentRootedObject m_ModuleObject;
	};

	using RegistryType = std::unordered_map<VfsPath, CompiledModule>;

	class Future
	{
	public:
		struct Evaluating
		{
			JS::PersistentRootedObject moduleNamespace;
			JS::PersistentRootedObject fulfill;
			JS::PersistentRootedObject reject;
		};
		struct Fulfilled
		{
			JS::PersistentRootedObject moduleNamespace;
		};
		struct Rejected
		{
			std::exception_ptr error;
		};
		struct Invalid {};
		using Status = std::variant<Evaluating, Fulfilled, Rejected, Invalid>;

		explicit Future(const ScriptRequest& rq, ModuleLoader& loader, const VfsPath& modulePath);
		Future() = default;
		Future(const Future&) = delete;
		Future& operator=(const Future&) = delete;
		Future(Future&& other) noexcept;
		Future& operator=(Future&& other) noexcept;
		~Future();

		[[nodiscard]] bool IsDone() const noexcept;

		/**
		 * Throws if the evaluation of the module failed.
		 * @return The module namespace. All exported values are a property
		 *	of this object. @c default is a property with name "default".
		 */
		[[nodiscard]] JSObject* Get();

	private:
		// It's save to not require a `JS::HandleValue` here.
		void SetReservedSlot(JS::Value privateValue) noexcept;

		Status m_Status{Invalid{}};
	};

	/**
	 * Load the specified module and all module it imports recursively.
	 *
	 * @param rq @c globalThis is taken from this @c ScriptRequest.
	 * @param modulePath The path to the file which should be loaded as a module.
	 * @return A future that is fulfilled when the evaluation of the module
	 *	completes.
	 */
	[[nodiscard]] Future LoadModule(const ScriptRequest& rq, const VfsPath& modulePath);

private:
	// Functions used by the `ScriptContext`.
	[[nodiscard]] static JSObject* ResolveHook(JSContext* cx, JS::HandleValue,
		JS::HandleObject moduleRequest) noexcept;
	[[nodiscard]] static bool DynamicImportHook(JSContext* cx, JS::HandleValue referencingPrivate,
		JS::HandleObject moduleRequest, JS::HandleObject promise) noexcept;

	RegistryType m_Registry;
};
} // namespace Script

#endif // INCLUDED_SCRIPTMODULELOADER
