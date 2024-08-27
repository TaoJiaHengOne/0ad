---

name: "Release Process Task"
about: "This is a special issue template for planning Alpha releases. DO NOT USE it for normal issue reports."
title: "[RELEASE PROCESS] Alpha XX"
labels:

- "Type/Task"
- "Theme/Build & Packages"

---

*Please fill out relevant information in the next line, set Milestone to the relevant Alpha and Branch/Tag to the relevant release branch when it's created. Then delete this line.*

# Alpha XX Release Process

This task tracks the progress of the next Alpha release. **Please do not report issues with the nightly build or with release candidates here:** instead create a new issue and set its Milestone to the relevant Alpha.

All details about each step are documented in [ReleaseProcess](wiki/ReleaseProcess).

**Release Manager:** @
**Translations Officer:** @

## Outstanding Issues

**All the following issues must be fixed in `main` (which closes the issue) and then cherry-picked to the release branch (after that, you can tick the checkbox below).**

Here are the Release Blocking issues currently delaying the release:

- [x] None currently

## Progress Tracking

### Release Branching

- [ ] [Test the tutorials](wiki/ReleaseProcess#test-the-tutorials)
- [ ] [Organize a first staff match](wiki/ReleaseProcess#organize-a-first-staff-match)
- [ ] [Prepare for branching](wiki/ReleaseProcess#prepare-for-branching)
- [ ] [Create a `release-aXX` branch](wiki/ReleaseProcess#create-a-release-axx-branch)
- [ ] [Adapt Jenkins for the release](wiki/ReleaseProcess#adapt-jenkins-for-the-release)
- [ ] [Prepare next multiplayer lobby in `main`](wiki/ReleaseProcess#prepare-next-multiplayer-lobby-in-main)
- [ ] [Generate next signing key for mods in `main`](wiki/ReleaseProcess#generate-next-signing-key-for-mods-in-main)
- [ ] [Start writing release announcement](wiki/ReleaseProcess#start-writing-release-announcement)
- [ ] [Start creating the release video](wiki/ReleaseProcess#start-creating-the-release-video)

### String Freeze

- [ ] [Announce string freeze](wiki/ReleaseProcess#announce-string-freeze)
- [ ] [Long strings check](wiki/ReleaseProcess#long-strings-check)

### Commit Freeze

- [ ] [Translation check](wiki/ReleaseProcess#translation-check)
- [ ] [Decide on included translations](wiki/ReleaseProcess#decide-on-included-translations)
- [ ] [Organize another staff match](wiki/ReleaseProcess#organize-another-staff-match)

---

Before moving on with Full Freeze, make sure that:

- [ ] At least 10 days have passed since the string freeze announcement
- [ ] Only this ticket remains in the Milestone
- [ ] All previous checkboxes are ticked

---

### Full Freeze

- [ ] [Freeze Jenkins](wiki/ReleaseProcess#freeze-jenkins)
- [ ] [Confirm full freeze](wiki/ReleaseProcess#confirm-full-freeze)
- [ ] [Package East Asian mods](wiki/ReleaseProcess#package-east-asian-mods)
- [ ] [Announce Release Candidates](wiki/ReleaseProcess#announce-release-candidates)
- [ ] Release Testing: [link to RC]( )

### Release

- [ ] [Tag the release commit](wiki/ReleaseProcess#tag-the-release-commit)
- [ ] [Create torrents and checksum files](wiki/ReleaseProcess#create-torrents-and-checksum-files)
- [ ] [Upload to Sourceforge and IndieDB](wiki/ReleaseProcess#upload-to-sourceforge-and-indiedb)
- [ ] [Move the lobby](wiki/ReleaseProcess#move-the-lobby)
- [ ] [Publish announcement](wiki/ReleaseProcess#publish-announcement)
- [ ] [Notify packagers](wiki/ReleaseProcess#notify-packagers)
- [ ] [Post-Release](wiki/ReleaseProcess#post-release)
