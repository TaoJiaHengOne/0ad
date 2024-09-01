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

// This pipeline builds the game on Linux (with min and max supported versions of GCC and clang) and runs tests.

pipeline {
	// Stop previous build in pull requests, but not in branches
	options { disableConcurrentBuilds(abortPrevious: env.CHANGE_ID != null) }

	agent none
	stages {
		stage("Setup") {
			agent {
				node {
					label 'LinuxAgent'
					customWorkspace 'workspace/linux'
				}
			}
			steps {
				discoverGitReferenceBuild()
				sh "cd build/jenkins/dockerfiles/ && docker build -t buster-base -f buster-base.Dockerfile ."
			}
		}
		stage("Linux Build") {
			failFast true

			matrix {
				axes {
					axis {
						name 'JENKINS_COMPILER'
						values 'gcc7' //, 'gcc14', 'clang8', 'clang18'
					}
					axis {
						name 'JENKINS_PCH'
						values 'pch' //, 'no-pch'
					}
				}

				agent {
					dockerfile {
						label 'LinuxAgent'
						customWorkspace "workspace/${JENKINS_COMPILER}-${JENKINS_PCH}"
						dir 'build/jenkins/dockerfiles'
						filename "${JENKINS_COMPILER}.Dockerfile"
					}
				}

				stages {
					stage("Pre-build") {
						steps {
							sh "git lfs pull -I binaries/data/tests"
							sh "git lfs pull -I \"binaries/data/mods/_test.*\""

							sh "libraries/build-source-libs.sh -j1 2> ${JENKINS_COMPILER}-prebuild-errors.log"

							script {
								if (env.JENKINS_PCH == "no-pch") {
									sh "build/workspaces/update-workspaces.sh -j1 --jenkins-tests --without-pch 2>> ${JENKINS_COMPILER}-prebuild-errors.log"
								} else {
									sh "build/workspaces/update-workspaces.sh -j1 --jenkins-tests 2>> ${JENKINS_COMPILER}-prebuild-errors.log"
								}
							}
						}
						post {
							failure {
								echo (message: readFile (file: "${JENKINS_COMPILER}-prebuild-errors.log"))
							}
						}
					}

					stage("Debug Build") {
						steps {
							retry(2) {
								script {
									try { sh "cd build/workspaces/gcc/ && make -j1 config=debug" }
									catch(e) {
										sh "cd build/workspaces/gcc/ && make clean config=debug"
										throw e
									}
								}
							}
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
							retry(2) {
								script {
									try { sh "cd build/workspaces/gcc/ && make -j1 config=release" }
									catch(e) {
										sh "cd build/workspaces/gcc/ && make clean config=release"
										throw e
									}
								}
							}
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
			}

			post {
				always {
					node('LinuxAgent') { ws('workspace/linux') {
						recordIssues enabledForFailure: true, qualityGates: [[threshold: 1, type: 'NEW']], tools: [clang(), gcc()]
					}}
				}
			}
		}
	}
}
