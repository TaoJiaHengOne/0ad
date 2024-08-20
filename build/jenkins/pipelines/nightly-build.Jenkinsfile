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

// This pipeline is used to generate the nightly builds.

def visualStudioPath = "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\MSBuild\\15.0\\Bin\\MSBuild.exe\""
def buildOptions = "/p:PlatformToolset=v141_xp /p:XPDeprecationWarning=false /t:pyrogenesis /t:AtlasUI /m:2 /nologo -clp:Warningsonly -clp:ErrorsOnly"

def gitHash = ""
def buildSPIRV = false

pipeline {
	agent {
		node {
			label 'WindowsAgent'
			customWorkspace 'workspace/nightly-build'
		}
	}

	parameters {
		booleanParam(name: 'NEW_REPO', defaultValue: false, description: 'If a brand new nightly repo is being generated, do not attempt to identify unchanged translations.')
		stashedFile(name: 'spirv_rules', description: 'rules.json file for generation of SPIR-V shaders. Needed for a new repo, else the existing rules files will be used. Uploading a new rules file will force-rebuild the shaders.')
	}

	stages {
		stage("Generate build version") {
			steps {
				checkout scmGit(branches: [[name: "${GIT_BRANCH}"]], extensions: [localBranch()])
				script { gitHash = bat(script:"@git rev-parse --short HEAD", returnStdout: true ).trim() }
				bat "cd build\\build_version && build_version.bat"
			}
		}

		stage("Pull game assets") {
			steps {
				bat "git lfs pull"
			}
		}

		stage("Check for shader changes") {
			when {
				changeset 'binaries/data/mods/**/shaders/**/*.xml'
			}
			steps {
				script { buildSPIRV = true }
			}
		}

		stage ("Pre-build") {
			steps {
				bat "cd libraries && get-windows-libs.bat"
				bat "(robocopy C:\\wxwidgets3.0.4\\lib libraries\\win32\\wxwidgets\\lib /MIR /NDL /NJH /NJS /NP /NS /NC) ^& IF %ERRORLEVEL% LEQ 1 exit 0"
				bat "(robocopy C:\\wxwidgets3.0.4\\include libraries\\win32\\wxwidgets\\include /MIR /NDL /NJH /NJS /NP /NS /NC) ^& IF %ERRORLEVEL% LEQ 1 exit 0"
				bat "cd build\\workspaces && update-workspaces.bat --atlas --without-pch --without-tests"
			}
		}

		stage ("Build") {
			steps {
				bat("cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Release ${buildOptions}")
			}
		}

		stage("Check-in SPIRV generation rules") {
			when {
				expression { env.spirv_rules_FILENAME }
			}
			steps {
				unstash 'spirv_rules'
				bat "move spirv_rules source\\tools\\spirv\\rules.json"
				script { buildSPIRV = true }
			}
		}

		stage("Mirror to SVN") {
			steps {
				ws("workspace/nightly-svn") {
					svn(url: "https://svn.wildfiregames.com/nightly-build/trunk", changelog: false)
					bat "svn revert -R . && svn cleanup"
					script { env.NIGHTLY_PATH = env.WORKSPACE }
				}
				bat """
				(robocopy . %NIGHTLY_PATH% ^
					/XD .git ^
					/XF .gitattributes ^
					/XF .gitignore ^
					/XD %cd%\\binaries\\system ^
					/XD %cd%\\build\\workspaces\\vs2017 ^
					/XD %cd%\\libraries\\source\\.svn ^
					/XD %cd%\\libraries\\win32\\.svn ^
					/XD %cd%\\libraries\\win32\\wxwidgets\\include ^
					/XD %cd%\\libraries\\win32\\wxwidgets\\lib ^
					/XD .svn ^
					/XD %NIGHTLY_PATH%\\binaries\\data\\mods\\mod\\shaders\\spirv ^
					/XD %NIGHTLY_PATH%\\binaries\\data\\mods\\public\\shaders\\spirv ^
				/MIR /NDL /NJH /NJS /NP /NS /NC) ^& IF %ERRORLEVEL% LEQ 1 exit 0
				"""
				bat """
				(robocopy binaries\\system ..\\nightly-svn\\binaries\\system ^
					/XF *.exp ^
					/XF *.lib ^
				/MIR /NDL /NJH /NJS /NP /NS /NC) ^& IF %ERRORLEVEL% LEQ 1 exit 0
				"""
			}
		}

		stage("Recompile SPIR-V shaders") {
			when {
				expression { buildSPIRV }
			}
			steps {
				ws("workspace/nightly-svn") {
					bat "python source/tools/spirv/compile.py -d binaries/data/mods/mod binaries/data/mods/mod source/tools/spirv/rules.json binaries/data/mods/mod"
					bat "python source/tools/spirv/compile.py -d binaries/data/mods/mod binaries/data/mods/public source/tools/spirv/rules.json binaries/data/mods/public"
				}
			}
		}

		stage("Update translations") {
			steps {
				ws("workspace/nightly-svn") {
					bat "cd source\\tools\\i18n && python updateTemplates.py"
					withCredentials([string(credentialsId: 'TX_TOKEN', variable: 'TX_TOKEN')]) {
						bat "cd source\\tools\\i18n && python pullTranslations.py"
					}
					bat "cd source\\tools\\i18n && python generateDebugTranslation.py --long"
					bat "cd source\\tools\\i18n && python cleanTranslationFiles.py"
					script { if (!params.NEW_REPO) {
						bat "python source\\tools\\i18n\\checkDiff.py --verbose"
					}}
					bat "cd source\\tools\\i18n && python creditTranslators.py"
				}
			}
		}

		stage("Commit") {
			steps {
				ws("workspace/nightly-svn") {
					bat "svn add --force ."
					bat "(for /F \"tokens=* delims=! \" %%A in ('svn status ^| findstr /R \"^!\"') do (svn delete %%A)) || (echo No deleted files found) ^& exit 0"
					withCredentials([usernamePassword(credentialsId: 'nightly-autobuild', passwordVariable: 'SVNPASS', usernameVariable: 'SVNUSER')]) {
						script { env.GITHASH = gitHash
							bat 'svn commit --username %SVNUSER% --password %SVNPASS% --no-auth-cache --non-interactive -m "Nightly build for %GITHASH% (%DATE%)"'
						}
					}
				}
			}
		}
	}
}
