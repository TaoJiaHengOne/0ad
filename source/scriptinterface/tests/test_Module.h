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
#include "scriptinterface/ModuleLoader.h"
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
		script.GetModuleLoader().LoadModule(rq, "include/entry.js");

		// This test does not rely on export to the engine. So use the logger to check if it succeeded.
		TS_ASSERT_STR_CONTAINS(logger.GetOutput(),"Test succeeded");
	}

	void test_Sequential()
	{
		{
			ScriptInterface script{"Test", "Test", g_ScriptContext};
			const ScriptRequest rq{script};
			script.GetModuleLoader().LoadModule(rq, "empty.js");
		}
		{
			ScriptInterface script{"Test", "Test", g_ScriptContext};
			const ScriptRequest rq{script};
			script.GetModuleLoader().LoadModule(rq, "empty.js");
		}
	}

	void test_Stacked()
	{
		ScriptInterface scriptOuter{"Test", "Test", g_ScriptContext};
		const ScriptRequest rqOuter{scriptOuter};
		{
			ScriptInterface scriptInner{"Test", "Test", g_ScriptContext};
			const ScriptRequest rqInner{scriptInner};
			scriptInner.GetModuleLoader().LoadModule(rqInner, "empty.js");
		}
		scriptOuter.GetModuleLoader().LoadModule(rqOuter, "empty.js");
	}

	void test_ImportInFunction()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		TestLogger logger;
		TS_ASSERT_THROWS(script.GetModuleLoader().LoadModule(rq, "import_inside_function.js"),
			const std::invalid_argument&);
		const std::string log{logger.GetOutput()};
		TS_ASSERT_STR_CONTAINS(log, "import_inside_function.js line 3");
		TS_ASSERT_STR_CONTAINS(log, "import declarations may only appear at top level of a module");
	}

	void test_NonExistent()
	{
		ScriptInterface script{"Test", "Test", g_ScriptContext};
		const ScriptRequest rq{script};

		const TestLogger _;
		TS_ASSERT_THROWS(script.GetModuleLoader().LoadModule(rq, "nonexistent.js"),
			const std::runtime_error&);
	}
};
