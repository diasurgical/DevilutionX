# DevilutionX Contribution Guide
Welcome! This document outlines the process and expectations for contributing to DevilutionX. Our goal is to keep the codebase clean, the project moving, and our contributors empowered.

## 🛠 For Developers (Submitting Pull Requests)
If you are writing code or adding assets, follow these steps to ensure a smooth review process:

### Code style guide
[The code style guide](https://github.com/diasurgical/devilutionX/wiki/Code-Style) is evolving with the project.

### C++ Standard
Despite setting C++ standard to 20 in CMakeLists.txt, features from this version are not being used.
The oldest compiler used is GCC 6.5 - and that defines our C++ feature set (meaning most of C++17).
It's present only to take advantage of fmt::format build time errors.

### PR Description & Context
- **Summarize Changes**: Clearly explain what your PR does and why.
- **Visuals are Key**: If your change affects the UI or graphics, you must include screenshots or a short GIF/Video. This helps reviewers understand the impact without having to compile and run the code immediately.
- **Link Issues**: If your PR fixes a known bug, include Closes #123 in the description.

### Branch & Commit Hygiene
- **Atomic Commits**: Try to keep commits logical.
- **The "Squash" Rule**: If your PR branch contains many "fixup," "typo," or "update" commits, the maintainers will likely Squash and Merge your PR to keep the master history clean.
- **Maintainable History**: If you want your specific commit history preserved (Rebase and Merge), ensure every commit is meaningful and the branch is kept up to date with the master branch.

### Rights & Assets
- **Ownership**: Any new assets (sounds, graphics, music) must have a clear rights check. State the source and the license in the PR description.

## 🎓 For Contributors (Reviewing & Merging)
Contributors are trusted members of the community with write access. Your role is to ensure quality and help move the project forward.

### Reviewing with Confidence
- **Tap a [Specialist](#-specialist-directory)**: If you are unsure about a specific area (e.g., low-level C++ performance or rendering logic), don't hesitate to tag a fellow contributor who specializes in that area.
- **Co-Reviewing**: It is perfectly okay to ask for a "second set of eyes" on Discord before hitting the merge button.
- **Trust the Process**: If a PR is green (passes CI), has been reviewed by at least one person, and doesn't represent a "controversial" change, feel empowered to merge it.
### Merging Protocols
- **Staleness**: If a PR has gone stale (conflicts with master), ask the developer to rebase. If they are unresponsive, you may perform the rebase/fix yourself if the contribution is valuable.
- **Version Management**: For standard bug fixes and QoL improvements, merging directly to master is the default. Major architectural changes should be discussed with the project owners first.

## 👥 Specialist Directory
If you need a specialized review, you can find our key contributors here. Feel free to @mention them in your PR or ping them on Discord.

| Area of Specialty | GitHub Handle | Discord Tag |
| :--- | :--- | :--- |
| **Graphics & Rendering** | [@GfxExpert](https://github.com/example) | `GfxGuru#1234` |
| **Networking & Multiplayer** | [@NetWizard](https://github.com/example) | `LagFree#5678` |
| **Save Games & Core Logic** | [@DataMaster](https://github.com/example) | `SaveState#9012` |
| **Asset Pipeline / CLX** | [@AssetKing](https://github.com/example) | `SpriteLord#3456` |


_“A project is only as strong as its contributors. Thank you for helping us keep Diablo 1 alive and better than ever!”_
