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

// This pipeline builds the game on Windows (which uses the MSVC 15.0 compiler from Visual Studio 2017) and runs tests.

def visualStudioPath = "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\MSBuild\\15.0\\Bin\\MSBuild.exe\""
def buildOptions = "/p:PlatformToolset=v141_xp /p:XPDeprecationWarning=false /t:pyrogenesis /t:AtlasUI /t:test /m:2 /nologo -clp:NoSummary"

pipeline {
	// Stop previous build in pull requests, but not in branches
	options { disableConcurrentBuilds(abortPrevious: env.CHANGE_ID != null) }

	agent {
		node {
			label 'WindowsAgent'
			customWorkspace 'workspace/vs2017'
		}
	}

	stages {
		stage ("Pre-build") {
			steps {
				discoverGitReferenceBuild()

				bat "git lfs pull -I binaries/data/tests"
				bat "git lfs pull -I \"binaries/data/mods/_test.*\""

				bat "cd libraries && get-windows-libs.bat"
				bat "(robocopy /MIR /NDL /NJH /NJS /NP /NS /NC C:\\wxwidgets3.0.4\\lib libraries\\win32\\wxwidgets\\lib) ^& IF %ERRORLEVEL% LEQ 1 exit 0"
				bat "(robocopy /MIR /NDL /NJH /NJS /NP /NS /NC C:\\wxwidgets3.0.4\\include libraries\\win32\\wxwidgets\\include) ^& IF %ERRORLEVEL% LEQ 1 exit 0"
				bat "cd build\\workspaces && update-workspaces.bat --atlas --jenkins-tests"
			}
		}

		stage("Debug Build") {
			steps {
				bat("cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Debug ${buildOptions}")
				timeout(time: 15) {
					bat "cd binaries\\system && test_dbg.exe > cxxtest-debug.xml"
				}
			}
			post {
				always {
					junit 'binaries/system/cxxtest-debug.xml'
				}
			}
		}

		stage ("Release Build") {
			steps {
				bat("cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Release ${buildOptions}")
				timeout(time: 5) {
					bat "cd binaries\\system && test.exe > cxxtest-release.xml"
				}
			}
			post {
				always {
					junit 'binaries/system/cxxtest-release.xml'
				}
			}
		}
	}

	post {
		always {
			recordIssues enabledForFailure: true, qualityGates: [[threshold: 1, type: 'NEW']], tool: msBuild()
		}
	}
}
