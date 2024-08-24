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

// This pipeline builds the game on macOS (which uses the clang-10 compiler) and runs tests.

pipeline {
	// Stop previous build in pull requests, but not in branches
	options { disableConcurrentBuilds(abortPrevious: env.CHANGE_ID != null) }

	agent {
		node {
			label 'macOSAgent'
			customWorkspace 'workspace/clang10'
		}
	}

	stages {
		stage ("Pre-build") {
			steps {
				discoverGitReferenceBuild()

				sh "git lfs pull -I binaries/data/tests"
				sh "git lfs pull -I \"binaries/data/mods/_test.*\""

				sh "libraries/build-macos-libs.sh -j4 2> macos-prebuild-errors.log"
				sh "build/workspaces/update-workspaces.sh -j4 --jenkins-tests 2>> macos-prebuild-errors.log"
			}
			post {
				failure {
					echo (message: readFile (file: "macos-prebuild-errors.log"))
				}
			}
		}

		stage("Debug Build") {
			steps {
				sh "cd build/workspaces/gcc/ && make -j4 config=debug"
				timeout(time: 15) {
					sh "cd binaries/system/ && ./test_dbg > cxxtest-debug.xml"
				}
			}
			post {
				always {
					junit 'binaries/system/cxxtest-debug.xml'
				}
			}
		}

		stage("Release Build") {
			steps {
				sh "cd build/workspaces/gcc/ && make -j4 config=release"
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
