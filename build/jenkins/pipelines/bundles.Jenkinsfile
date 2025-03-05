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

// This pipeline is used to generate bundles (Windows installer, macOS package, and source tarballs).

pipeline {
	agent {
		node {
			label 'macOSAgentVentura'
		}
	}

	// Archive the installer for public download; keep only the latest one.
	options {
		buildDiscarder logRotator(artifactNumToKeepStr: '1')
		skipDefaultCheckout true
	}

	parameters {
		string(name: 'BUNDLE_VERSION', defaultValue: '0.28.0dev', description: 'Bundle Version')
		booleanParam(name: 'DO_GZIP', defaultValue: true, description: 'Create .gz unix tarballs as well as .xz')
	}

	environment {
		MIN_OSX_VERSION = "10.15"
	}

	stages {
		stage("Checkout Nightly Build") {
			steps {
				checkout changelog: false, poll: false, scm: [
					$class: 'SubversionSCM',
					locations: [[local: '.', remote: 'https://svn.wildfiregames.com/nightly-build/trunk']],
					quietOperation: false,
					workspaceUpdater: [$class: 'UpdateWithCleanUpdater']]
				sh "svn cleanup"
			}
		}

		stage("Compile Native macOS Executable") {
			steps {
				sh "cd libraries/ && MIN_OSX_VERSION=${env.MIN_OSX_VERSION} ./build-macos-libs.sh ${JOBS} --force-rebuild"
				sh "cd build/workspaces/ && ./update-workspaces.sh --macosx-version-min=${env.MIN_OSX_VERSION}"
				sh "cd build/workspaces/gcc/ && make ${JOBS}"
				sh "svn cleanup --remove-unversioned build"
				sh "svn cleanup --remove-unversioned libraries"
			}
		}

		stage("Create Mod Archives") {
			steps {
				sh "source/tools/dist/build-archives.sh"
			}
		}

		stage("Create Native macOS Bundle") {
			steps {
				withCredentials([
					string(credentialsId: 'apple-keychain', variable: 'KEYCHAIN_PW'),
					string(credentialsId: 'apple-signing', variable: 'SIGNKEY_SHA'),
					usernamePassword(credentialsId: 'apple-notarization', passwordVariable: 'NOTARIZATION_PW', usernameVariable: 'NOTARIZATION_USER')])
				{
					sh '''
						security unlock-keychain -p ${KEYCHAIN_PW} login.keychain
						/opt/wfg/venv/bin/python3 source/tools/dist/build-osx-bundle.py \
							--min_osx=${MIN_OSX_VERSION} \
							-s ${SIGNKEY_SHA} \
							--notarytool_user=${NOTARIZATION_USER} \
							--notarytool_team=P7YF26GARW \
							--notarytool_password=${NOTARIZATION_PW} \
							${BUNDLE_VERSION}
					'''
				}
			}
		}

		stage("Compile Intel macOS Executable") {
			environment {
				ARCH = "x86_64"
				HOSTTYPE = "x86_64"
			}
			steps {
				sh "cd libraries/ && MIN_OSX_VERSION=${env.MIN_OSX_VERSION} ./build-macos-libs.sh ${JOBS} --force-rebuild"
				sh "cd build/workspaces/ && ./update-workspaces.sh --macosx-version-min=${env.MIN_OSX_VERSION}"
				sh "cd build/workspaces/gcc/ && make clean"
				sh "cd build/workspaces/gcc/ && make ${JOBS}"
				sh "svn cleanup --remove-unversioned build"
				sh "svn cleanup --remove-unversioned libraries"
			}
		}

		stage("Create Intel macOS Bundle") {
			steps {
				withCredentials([
					string(credentialsId: 'apple-keychain', variable: 'KEYCHAIN_PW'),
					string(credentialsId: 'apple-signing', variable: 'SIGNKEY_SHA'),
					usernamePassword(credentialsId: 'apple-notarization', passwordVariable: 'NOTARIZATION_PW', usernameVariable: 'NOTARIZATION_USER')])
				{
					sh '''
						security unlock-keychain -p ${KEYCHAIN_PW} login.keychain
						/opt/wfg/venv/bin/python3 source/tools/dist/build-osx-bundle.py \
							--architecture=x86_64 \
							--min_osx=${MIN_OSX_VERSION} \
							-s ${SIGNKEY_SHA} \
							--notarytool_user=${NOTARIZATION_USER} \
							--notarytool_team=P7YF26GARW \
							--notarytool_password=${NOTARIZATION_PW} \
							${BUNDLE_VERSION}
					'''
				}
			}
		}

		stage("Create Windows Installer & Tarballs") {
			steps {
				sh "BUNDLE_VERSION=${params.BUNDLE_VERSION} DO_GZIP=${params.DO_GZIP} source/tools/dist/build-unix-win32.sh"
			}
		}
	}

	post {
		success {
			archiveArtifacts '*.dmg,*.exe,*.tar.gz,*.tar.xz'
			sh "shasum -a 1 *.{dmg,exe,tar.gz,tar.xz}"
		}
	}
}
