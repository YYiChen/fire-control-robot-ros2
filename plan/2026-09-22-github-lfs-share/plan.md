# GitHub private sharing with LFS

## Goal

Create a private GitHub repository containing the project while preserving the 103.44 MiB thesis source document through Git LFS so that the remote accepts the complete project history.

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
- `plan/2026-09-22-github-lfs-share/plan.md`
- `plan/log.md`
- local Git history and `origin` remote configuration required for this sharing target

## Read-Only Files

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- all source, reference, model, and documentation payload files

## Shared Dependencies

- GitHub repository `YYiChen/fire-control-robot-ros2`, private
- GitHub's 100 MiB normal Git blob limit
- Git LFS client available locally

## Expected Work

1. Add the thesis `.docx` pattern to Git LFS and migrate existing local history to LFS pointers.
2. Commit only the LFS tracking metadata and target records.
3. Push `main` through the active local proxy and confirm a remote commit exists.

## Validation

- `git diff --check`
- `git status --short --branch`
- `git lfs ls-files` confirms the thesis document is an LFS object.
- GitHub REST commit query and `git ls-remote` confirm a non-empty `main` branch.

These checks cover the actual remote-sharing behavior and the GitHub size limit that blocked the initial push. No ROS test is relevant because this change does not alter runtime code or configuration.

## Experience Signal (for human review)

- GitHub rejected risk was found before upload completion: the thesis source document is over the normal per-file limit, so an LFS rule is required for a complete shared repository.

## Commit Intent

```text
chore: track thesis source through git lfs
```
