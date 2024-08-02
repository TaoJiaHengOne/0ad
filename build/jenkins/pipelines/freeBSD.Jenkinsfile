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

// This pipeline builds the game on FreeBSD (which uses the clang-17 compiler) and runs tests.
// Due to a compilation bug with SpiderMonkey, the game is only built with the release configuration.

pipeline {
	agent {
		node {
			label 'FreeBSDAgent'
			customWorkspace 'workspace/clang17'
		}
	}
	environment {
		USE = 'iconv'
		WX_CONFIG = '/usr/local/bin/wxgtk3u-3.0-config'
		CXXFLAGS = '-stdlib=libc++ '
		LLVM_OBJDUMP = '/usr/bin/llvm-objdump'
	}
	stages {
		stage ("Pre-build") {
			steps {
				discoverGitReferenceBuild()

				sh "git lfs pull -I binaries/data/tests"
				sh "git lfs pull -I \"binaries/data/mods/_test.*\""

				sh "libraries/build-source-libs.sh -j2 2> freebsd-prebuild-errors.log"
				sh "build/workspaces/update-workspaces.sh -j2 --jenkins-tests 2>> freebsd-prebuild-errors.log"
			}
			post {
				failure {
					echo (message: readFile (file: "freebsd-prebuild-errors.log"))
				}
			}
		}

		stage ("Release Build") {
			steps {
				sh "cd build/workspaces/gcc/ && gmake -j2 config=release"

				timeout(time: 15) {
					sh "cd binaries/system/ && ./test > cxxtest-release.xml"
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
			recordIssues enabledForFailure: true, qualityGates: [[threshold: 1, type: 'NEW']], tool: clang()
		}
	}
}
