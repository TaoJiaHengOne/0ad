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

// This pipeline builds the game on Linux (with minimum supported versions of GCC and clang) and runs tests.

pipeline {
	// Stop previous build in pull requests, but not in branches
	options { disableConcurrentBuilds(abortPrevious: env.CHANGE_ID != null) }

	parameters {
		booleanParam description: 'Non-incremental build', name: 'CLEANBUILD'
	}

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
				sh "cd build/jenkins/dockerfiles/ && docker build -t bullseye-base -f bullseye-base.Dockerfile ."
			}
		}
		stage("Linux Build") {
			failFast true

			matrix {
				axes {
					axis {
						name 'JENKINS_COMPILER'
						values 'gcc8', 'clang9'
					}
				}

				agent {
					dockerfile {
						label 'LinuxAgent'
						customWorkspace "workspace/${JENKINS_COMPILER}-pch"
						dir 'build/jenkins/dockerfiles'
						filename "${JENKINS_COMPILER}.Dockerfile"
						// Prevent Jenkins from running commands with the UID of the host's jenkins user
						// https://stackoverflow.com/a/42822143
						args '-u root'
					}
				}

				stages {
					stage("Pre-build") {
						steps {
							sh "git lfs pull -I binaries/data/tests"
							sh "git lfs pull -I \"binaries/data/mods/_test.*\""

							sh "libraries/build-source-libs.sh 2> ${JENKINS_COMPILER}-prebuild-errors.log"

							sh "build/workspaces/update-workspaces.sh --jenkins-tests 2>> ${JENKINS_COMPILER}-prebuild-errors.log"

							script {
								if (params.CLEANBUILD) {
									sh "cd build/workspaces/gcc/ && make clean config=debug"
									sh "cd build/workspaces/gcc/ && make clean config=release"
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
							sh "cd build/workspaces/gcc/ && make config=debug"
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
							sh "cd build/workspaces/gcc/ && make config=release"
						}
					}

					stage("Release Tests") {
						steps {
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
