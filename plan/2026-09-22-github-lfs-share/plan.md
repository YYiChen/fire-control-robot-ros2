# GitHub private sharing with LFS

## Goal

Create a private GitHub repository containing the project while excluding both source Word documents from the shared Git history and preserving their local copies.

## Dirty-State Note

Starting state:

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

The two untracked `lab_room` files belong to the user's current scene work and are outside this target. They must not be staged, modified, or included in the push.

## Owned Files

- `.gitattributes`
- `.gitignore`
- `plan/2026-09-22-github-lfs-share/plan.md`
- `plan/log.md`
- local Git history and `origin` remote configuration required for this sharing target

## Read-Only Files

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- all source, reference, model, and documentation payload files

## Shared Dependencies

- GitHub repository `YYiChen/fire-control-robot-ros2`, private
- GitHub repository `YYiChen/fire-control-robot-ros2`, private
- the user's instruction that Word papers must not be shared

## Expected Work

1. Preserve local copies of the two `.docx` files outside the Git rewrite area.
2. Remove both documents from all Git history and restore them locally as ignored/untracked source material.
3. Update the target record, push `main` through the active local proxy, and confirm a remote commit exists.

## Validation

- `git diff --check`
- `git status --short --branch`
- `git log --all -- <docx paths>` returns no shared-history entries.
- local file checks confirm the papers remain available after the history rewrite.
- GitHub REST commit query and `git ls-remote` confirm a non-empty `main` branch.

These checks cover the actual remote-sharing behavior, the user-requested exclusion, and preservation of the local paper files. No ROS test is relevant because this change does not alter runtime code or configuration.

## Experience Signal (for human review)

- The original attempt identified the 103.44 MiB paper as exceeding GitHub's normal blob limit. The user then chose exclusion from the shared repository, which takes precedence over using LFS.

## Commit Intent

```text
chore: exclude papers from shared repository
```
