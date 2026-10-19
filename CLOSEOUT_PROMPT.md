# Repository maintenance handoff, 19 October 2026

## Objective and verified baseline

Preserve existing source, reconcile completed work, and retire individually verified redundant branches, linked checkouts and owner-approved ignored build output. The owner explicitly confirmed that the inventoried ignored files were disposable build outputs. No product feature, build, installer, release, deployment or host power action belongs to this pass.

The primary checkout was clean before the pass. It was fast-forwarded by 455 existing commits to `ebf4158dd607c7fcdba0dcb7bf1af7977d882528`, which matched the remote main ref. Every one of the ten local delivery branch tips was already an ancestor of that remote main. No source merge or history rewrite was needed. A complete owner-managed source and Git-administration backup passed inventory readback and archive integrity verification before cleanup. Ignored build output was excluded from that backup.

## Completed cleanup

- Removed all ten `task/delivery-*-20261005` remote branches after proving their remote and local tips were ancestors of remote main. Their absence was read back with `git ls-remote`.
- Removed the corresponding ten linked checkouts and ten local branches through normal Git worktree and branch removal commands.
- Exactly one local checkout and one local branch, `main`, remain. No stashes existed, and the worktree metadata dry-run reported no stale entries.
- Removed the approved ignored build output except for the three retained dependency source directories listed below. The initial ignored inventory contained 581,887 file-path entries, including paths reached through directory links. That is an inventory count, not a claim about distinct physical files or bytes reclaimed.
- Tracked source matched HEAD after recovery and before these maintenance records were edited. No application code or current screenshots were intentionally changed.

## Cleanup incident and verified recovery

The first file-removal implementation followed an ignored build-resource directory link into tracked resources and deleted 6,853 tracked files. Removal was stopped immediately. The deleted files were restored from the unchanged HEAD. A further 34 metadata entries had identical HEAD blob hashes; refreshing their index entries left both the index and tracked working tree clean. No source recovery commit was required. Subsequent cleanup unlinked directory reparse points without traversing their targets before removing ignored files. This incident was not an external intrusion.

## Retained work and blockers

Fifty-eight other remote non-main branches remain. Fifty-one already-merged refs were retained because this pass did not establish their ownership and retirement eligibility. These seven refs contain history outside main and remain preserved without integrating unfinished work:

- `gap/lan-drop-app-w1`
- `gap/lan-drop-integrate-w1`
- `gap/lan-drop-site-w1`
- `gap/motion-tab-w5`
- `gap/vocab-limits-w5`
- `integrate/lan-drop`
- `recovery/reinstall-startup-tracing-20261013`

Three ignored dependency source directories remain in the primary checkout:

- `deps/build/dep_OpenEXR-prefix/src/dep_OpenEXR/`
- `deps/build/dep_ZLIB-prefix/src/dep_ZLIB/`
- `deps/build/dep_wxWidgets-prefix/src/dep_wxWidgets/`

Automatic approval review rejected recursive removal of these three directories with `blocked by policy`, without a narrower reason. It also rejected removal of redundant temporary archive files. Those items were retained; this handoff does not claim that every disposable file was removed. Do not retry the rejected operations through a different execution route.

## Verification and next steps

Verification covered clean tracked state, source-tip ancestry, remote branch absence, archive integrity and inventory readback, worktree inventory, local branch inventory and the absence of stashes. No product build, installer, test suite or runtime acceptance was run, and no release or new CI verdict is claimed. Existing product issues and unfinished acceptance remain open.

This record captures state before its own maintenance commit. Verify that commit on remote main separately. Retain the backup and unfinished branches. Review the remaining remote refs before any future retirement; the remaining blocked deletion targets need an allowed removal route or an external state change. Product acceptance work continues from the existing issue handoffs, including issue 60, and is outside this maintenance pass.
