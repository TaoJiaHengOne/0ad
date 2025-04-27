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

// This pipeline is used to generate patched releases, suitable to be bundled and distributed.

def visualStudioPath = '"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"'
def buildOptions = '/p:PlatformToolset=v141_xp /p:XPDeprecationWarning=false /t:pyrogenesis /t:AtlasUI %JOBS% /nologo -clp:Warningsonly -clp:ErrorsOnly'

pipeline {
    agent {
        node {
            label 'WindowsAgent'
        }
    }

    parameters {
        string(name: 'BUNDLE_VERSION', description: 'Bundle Version')
        string(name: 'NIGHTLY_REVISION', description: 'Revision of the nightly build corresponding to the release')
        string(name: 'RELEASE_TAG', description: 'Git tag from which the point release originates')
    }

    stages {
        stage('Generate Patch') {
            steps {
                checkout scmGit(branches: [[name: "${GIT_BRANCH}"]], extensions: [cleanAfterCheckout(), localBranch()])
                bat 'cd build\\build_version && build_version.bat'
                stash includes: 'build/build_version/build_version.txt', name: 'build_version'
                bat "git diff ${RELEASE_TAG}..HEAD > ${BUNDLE_VERSION}.patch"
                stash includes: "${params.BUNDLE_VERSION}.patch", name: 'patch'
                archiveArtifacts artifacts: "${params.BUNDLE_VERSION}.patch"
            }
        }

        stage('Patch Windows Build') {
            steps {
                ws('workspace/patch-release-svn') {
                    checkout changelog: false, poll: false, scm: [
                        $class: 'SubversionSCM',
                        locations: [[local: '.', remote: "https://svn.wildfiregames.com/nightly-build/trunk@${NIGHTLY_REVISION}"]],
                        quietOperation: false,
                        workspaceUpdater: [$class: 'UpdateWithCleanUpdater']]
                    unstash 'patch'
                    bat "svn patch ${params.BUNDLE_VERSION}.patch"

                    unstash 'build_version'
                    bat 'cd libraries && get-windows-libs.bat'

                    bat '(robocopy E:\\wxWidgets-3.2.6\\lib libraries\\win32\\wxwidgets\\lib /MIR /NDL /NJH /NJS /NP /NS /NC) ^& IF %ERRORLEVEL% LEQ 1 exit 0'
                    bat '(robocopy E:\\wxWidgets-3.2.6\\include libraries\\win32\\wxwidgets\\include /MIR /NDL /NJH /NJS /NP /NS /NC) ^& IF %ERRORLEVEL% LEQ 1 exit 0'
                    bat 'cd build\\workspaces && update-workspaces.bat --without-pch --without-tests'
                    bat "cd build\\workspaces\\vs2017 && ${visualStudioPath} pyrogenesis.sln /p:Configuration=Release ${buildOptions}"

                    script {
                        def modifiedFiles = bat(script:'@svn status', returnStdout: true).split('\n').collect { l -> l.drop(8).trim() }.join(', ')
                        tar archive: true, compress: true, exclude: '*.orig, binaries/system/*.exp, binaries/system/*.lib, build/workspaces/vs2017, libraries/win32/wxwidgets/**', file: "${params.BUNDLE_VERSION}-nightly-patch.tar.gz", glob: modifiedFiles
                    }
                }
            }

            post {
                cleanup {
                    ws('workspace/patch-release-svn') {
                        bat 'svn revert -R .'
                        bat 'svn cleanup --remove-unversioned'
                    }
                }
            }
        }

        stage('Bundle Patched Release') {
            steps {
                build job: '0ad-patch-bundles', wait: false, waitForStart: true, parameters: [
                    string(name: 'BUNDLE_VERSION', value: "${params.BUNDLE_VERSION}"),
                    string(name: 'NIGHTLY_REVISION', value: "${params.NIGHTLY_REVISION}"),
                    booleanParam(name: 'PATCH_BUILD', value: true)
                ]
            }
        }
    }
}
