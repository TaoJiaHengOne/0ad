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

// This pipeline builds the game on Windows (with the MSVC 15.0 compiler) and runs tests.

def visualStudioPath = "\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe\""
def buildOptions = "/p:PlatformToolset=v141_xp /p:XPDeprecationWarning=false /t:pyrogenesis /t:AtlasUI /t:test %JOBS% /nologo -clp:NoSummary"

pipeline {
	// Stop previous build in pull requests, but not in branches
	options { disableConcurrentBuilds(abortPrevious: env.CHANGE_ID != null) }

	parameters {
		booleanParam description: 'Non-incremental build', name: 'CLEANBUILD'
	}

	agent {
		node {
			label 'WindowsAgent'
			customWorkspace 'workspace/win32-pch'
		}
	}

	stages {
		stage ("Pre-build") {
			steps {
				discoverGitReferenceBuild()

				bat "git lfs pull -I binaries/data/tests"
				bat "git lfs pull -I \"binaries/data/mods/_test.*\""

				bat "cd libraries && get-windows-libs.bat"
				bat "(robocopy /MIR /NDL /NJH /NJS /NP /NS /NC E:\\wxWidgets-3.2.6\\lib libraries\\win32\\wxwidgets\\lib) ^& IF %ERRORLEVEL% LEQ 1 exit 0"
				bat "(robocopy /MIR /NDL /NJH /NJS /NP /NS /NC E:\\wxWidgets-3.2.6\\include libraries\\win32\\wxwidgets\\include) ^& IF %ERRORLEVEL% LEQ 1 exit 0"
				bat "cd build\\workspaces && update-workspaces.bat --jenkins-tests"

				script {
					if (params.CLEANBUILD) {
						bat "cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Debug /t:Clean"
						bat "cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Release /t:Clean"
					}
				}
			}
		}

		stage("Debug Build") {
			steps {
				bat "cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Debug ${buildOptions}"
			}
			post {
				failure {
					script { if (!params.CLEANBUILD) {
						build wait: false, job: "$JOB_NAME", parameters: [booleanParam(name: 'CLEANBUILD', value: true)]
					}}
				}
			}
		}

		stage("Debug Tests") {
			steps {
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
				bat "cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Release ${buildOptions}"
			}
		}

		stage ("Release Tests") {
			steps {
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
