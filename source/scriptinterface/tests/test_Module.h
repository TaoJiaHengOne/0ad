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

#include "lib/self_test.h"

#include "ps/CLogger.h"
#include "ps/Filesystem.h"
#include "scriptinterface/FunctionWrapper.h"
#include "scriptinterface/ModuleLoader.h"
#include "scriptinterface/Object.h"
#include "scriptinterface/Promises.h"
#include "scriptinterface/ScriptContext.h"
#include "scriptinterface/ScriptInterface.h"

class TestScriptModule : public CxxTest::TestSuite
{
public:
	void setUp()
	{
		g_VFS = CreateVfs();
		TS_ASSERT_OK(g_VFS->Mount(L"", DataDir() / "mods" / "_test.scriptinterface" / "module" / "",
			VFS_MOUNT_MUST_EXIST));
	}

	void tearDown()
	{
		g_VFS.reset();
	}

	void test_StaticImport()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		TestLogger logger;
		std::ignore = script.GetModuleLoader().LoadModule(rq, "include/entry.js");

		// This test does not rely on export to the engine. So use the logger to check if it succeeded.
		TS_ASSERT_STR_CONTAINS(logger.GetOutput(),"Test succeeded");
	}

	void test_Sequential()
	{
		{
			ScriptInterface script{"Test", "Test", g_ScriptContext};
			const ScriptRequest rq{script};
			std::ignore = script.GetModuleLoader().LoadModule(rq, "empty.js");
		}
		{
			ScriptInterface script{"Test", "Test", g_ScriptContext};
			const ScriptRequest rq{script};
			std::ignore = script.GetModuleLoader().LoadModule(rq, "empty.js");
		}
	}

	void test_Stacked()
	{
		ScriptInterface scriptOuter{"Test", "Test", g_ScriptContext};
		const ScriptRequest rqOuter{scriptOuter};
		{
			ScriptInterface scriptInner{"Test", "Test", g_ScriptContext};
			const ScriptRequest rqInner{scriptInner};
			std::ignore = scriptInner.GetModuleLoader().LoadModule(rqInner, "empty.js");
		}
		std::ignore = scriptOuter.GetModuleLoader().LoadModule(rqOuter, "empty.js");
	}

	void test_ImportInFunction()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		TestLogger logger;
		TS_ASSERT_THROWS(std::ignore = script.GetModuleLoader().LoadModule(rq,
			"import_inside_function.js"), const std::invalid_argument&);
		const std::string log{logger.GetOutput()};
		TS_ASSERT_STR_CONTAINS(log, "import_inside_function.js line 3");
		TS_ASSERT_STR_CONTAINS(log, "import declarations may only appear at top level of a module");
	}

	void test_NonExistent()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		const TestLogger _;
		TS_ASSERT_THROWS(std::ignore = script.GetModuleLoader().LoadModule(rq, "nonexistent.js"),
			const std::runtime_error&);
	}

	void test_EvaluateOnce()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		{
			TestLogger logger;
			std::ignore = script.GetModuleLoader().LoadModule(rq, "blabbermouth.js");
			TS_ASSERT_STR_CONTAINS(logger.GetOutput(), "blah blah blah");
		}
		{
			TestLogger logger;
			std::ignore = script.GetModuleLoader().LoadModule(rq, "include/../blabbermouth.js");
			TS_ASSERT_STR_NOT_CONTAINS(logger.GetOutput(), "blah blah blah");
		}
	}

	void test_TopLevelAwaitFinite()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};
		auto future = script.GetModuleLoader().LoadModule(rq, "top_level_await_finite.js");

		TS_ASSERT(!future.IsDone());
		g_ScriptContext->RunJobs();
		TS_ASSERT(future.IsDone());
		std::ignore = future.Get();
	}

	void test_TopLevelAwaitInfinite()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "top_level_await_infinite.js");

		g_ScriptContext->RunJobs();
		TS_ASSERT(!future.IsDone());
		TS_ASSERT_THROWS_ANYTHING(std::ignore = future.Get());
	}

	void test_MoveFulfilledFuture()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		Script::ModuleLoader::Future future0{
			script.GetModuleLoader().LoadModule(rq, "empty.js")};

		g_ScriptContext->RunJobs();
		TS_ASSERT(future0.IsDone());

		Script::ModuleLoader::Future future1{std::move(future0)};
		Script::ModuleLoader::Future future2;
		future2 = std::move(future1);

		TS_ASSERT(!future0.IsDone());
		TS_ASSERT(!future1.IsDone());
		TS_ASSERT(future2.IsDone());
	}

	void test_MoveEvaluatingFuture()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		Script::ModuleLoader::Future future0{
			script.GetModuleLoader().LoadModule(rq, "top_level_await_finite.js")};
		Script::ModuleLoader::Future future1{std::move(future0)};
		Script::ModuleLoader::Future future2;
		future2 = std::move(future1);

		TS_ASSERT(!future0.IsDone());
		TS_ASSERT(!future1.IsDone());
		TS_ASSERT(!future2.IsDone());
		g_ScriptContext->RunJobs();
		TS_ASSERT(!future0.IsDone());
		TS_ASSERT(!future1.IsDone());
		TS_ASSERT(future2.IsDone());
	}

	void test_EvaluateReplacedFuture()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		TestLogger logger;
		auto future{script.GetModuleLoader().LoadModule(rq, "delayed_blabbermouth.js")};
		TS_ASSERT_STR_NOT_CONTAINS(logger.GetOutput(), "blah blah blah");
		TS_ASSERT(!future.IsDone());

		future = script.GetModuleLoader().LoadModule(rq, "empty.js");
		TS_ASSERT(!future.IsDone());

		g_ScriptContext->RunJobs();
		TS_ASSERT(future.IsDone());
		TS_ASSERT_STR_CONTAINS(logger.GetOutput(), "blah blah blah");
	}

	void test_TopLevelThrow()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		// To silence the error.
		const TestLogger _;
		auto future = script.GetModuleLoader().LoadModule(rq, "top_level_throw.js");

		g_ScriptContext->RunJobs();
		TS_ASSERT(future.IsDone());
		TS_ASSERT_THROWS_EQUALS(std::ignore = future.Get(), const std::runtime_error& e, e.what(),
			"Error: Test reason\n@top_level_throw.js:1:7\n");
	}

	void test_Export()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "export.js");

		g_ScriptContext->RunJobs();
		JS::RootedObject ns{rq.cx, future.Get()};
		JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};

		{
			int value{0};
			TS_ASSERT(Script::GetProperty(rq, moduleValue, "value", value));
			TS_ASSERT_EQUALS(value, 6);
		}

		TS_ASSERT(ScriptFunction::CallVoid(rq, moduleValue, "mutate", 12));

		{
			int value{0};
			TS_ASSERT(Script::GetProperty(rq, moduleValue, "value", value));
			TS_ASSERT_EQUALS(value, 12);
		}
	}

	void test_ExportSame()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		{
			auto future = script.GetModuleLoader().LoadModule(rq, "export.js");
			g_ScriptContext->RunJobs();
			JS::RootedObject ns{rq.cx, future.Get()};
			JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};
			TS_ASSERT(ScriptFunction::CallVoid(rq, moduleValue, "mutate", 12));
		}

		{
			auto future = script.GetModuleLoader().LoadModule(rq, "include/../export.js");
			g_ScriptContext->RunJobs();
			JS::RootedObject ns{rq.cx, future.Get()};
			JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};
			int value{0};
			TS_ASSERT(Script::GetProperty(rq, moduleValue, "value", value));
			TS_ASSERT_EQUALS(value, 12);
		}
	}

	void test_ExportIndirect()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		{
			auto future = script.GetModuleLoader().LoadModule(rq, "export.js");
			g_ScriptContext->RunJobs();
			JS::RootedObject ns{rq.cx, future.Get()};
			JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};
			TS_ASSERT(ScriptFunction::CallVoid(rq, moduleValue, "mutate", 12));
		}

		{
			auto future = script.GetModuleLoader().LoadModule(rq, "indirect.js");
			g_ScriptContext->RunJobs();
			JS::RootedObject ns{rq.cx, future.Get()};
			JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};
			int value{0};
			TS_ASSERT(Script::GetProperty(rq, moduleValue, "value", value));
			TS_ASSERT_EQUALS(value, 12);
		}
	}

	void test_ExportDefaultImmutable()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "export_default/immutable.js");

		g_ScriptContext->RunJobs();

		JS::RootedObject ns{rq.cx, future.Get()};
		JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};

		int value{0};
		TS_ASSERT(Script::GetProperty(rq, moduleValue, "default", value));
		TS_ASSERT_EQUALS(value, 36);
	}

	void test_ExportDefaultInvalid()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		TestLogger logger;
		TS_ASSERT_THROWS(std::ignore = script.GetModuleLoader().LoadModule(rq,
			"export_default/invalid.js"), const std::invalid_argument&);
		const std::string log{logger.GetOutput()};
		TS_ASSERT_STR_CONTAINS(log, "export_default/invalid.js line 1");
	}

	void test_ExportDefaultDoesNotWorkAround()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "export_default/does_not_work_around.js");

		g_ScriptContext->RunJobs();

		JS::RootedObject ns{rq.cx, future.Get()};
		JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};

		int value{0};
		TS_ASSERT(Script::GetProperty(rq, moduleValue, "default", value));
		TS_ASSERT_DIFFERS(value, 36);
		TS_ASSERT_EQUALS(value, 6);
	}

	void test_ExportDefaultWorksAround()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "export_default/works_around.js");

		g_ScriptContext->RunJobs();

		JS::RootedObject ns{rq.cx, future.Get()};
		JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};

		int value{0};
		TS_ASSERT(Script::GetProperty(rq, moduleValue, "default", value));
		TS_ASSERT_EQUALS(value, 36);
	}

	void test_ReplaceEvaluatingFuture()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "top_level_await_finite.js");
		future = script.GetModuleLoader().LoadModule(rq, "export.js");

		g_ScriptContext->RunJobs();
		JS::RootedObject ns{rq.cx, future.Get()};
		JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};

		int value{0};
		TS_ASSERT(Script::GetProperty(rq, moduleValue, "value", value));
		TS_ASSERT_EQUALS(value, 6);
	}

	void test_DynamicImport()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "dynamic_import.js");

		g_ScriptContext->RunJobs();

		JS::RootedObject ns{rq.cx, future.Get()};
		JS::RootedValue moduleValue{rq.cx, JS::ObjectValue(*ns)};

		JS::RootedValue promise{rq.cx};
		TS_ASSERT(ScriptFunction::Call(rq, moduleValue, "default", &promise));
		TS_ASSERT(promise.isObject());
		JS::RootedObject promiseObject{rq.cx, &promise.toObject()};
		TS_ASSERT(JS::IsPromiseObject(promiseObject));

		TS_ASSERT_EQUALS(JS::GetPromiseState(promiseObject), JS::PromiseState::Pending);
		g_ScriptContext->RunJobs();
		TS_ASSERT_EQUALS(JS::GetPromiseState(promiseObject), JS::PromiseState::Fulfilled);

		JS::RootedValue piModule{rq.cx, JS::GetPromiseResult(promiseObject)};

		double pi{0.0};
		TS_ASSERT(Script::FromJSProperty(rq, piModule, "default", pi));

		TS_ASSERT_LESS_THAN(pi, 3.1416);
		TS_ASSERT_LESS_THAN(3.1415, pi);
	}

	void test_Meta()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		auto future = script.GetModuleLoader().LoadModule(rq, "meta.js");

		g_ScriptContext->RunJobs();

		JS::RootedObject ns{rq.cx, future.Get()};
		const JS::RootedValue modNamespace{rq.cx, JS::ObjectValue(*ns)};

		JS::RootedValue meta{rq.cx};
		TS_ASSERT(ScriptFunction::Call(rq, modNamespace, "getMeta", &meta));

		std::string path;
		TS_ASSERT(Script::GetProperty(rq, meta, "path", path));
		TS_ASSERT_STR_EQUALS(path, "meta.js");
	}
};
