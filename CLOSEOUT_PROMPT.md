# Reinstall preservation, 13 October 2026

Scope: preserve existing local source and refs, reconcile completed compatible contributions and retire individually proved redundant items. The owner explicitly skipped OneDrive archives and cloud archive readback. No product change, new build, installer, release, deployment or host power action belongs to this pass.

Current integration starts from main 02f4c2d121d6df891ba4deef3fa52828b143cfe3. The completed bootstrap history cdc6b2054ba7ad56faa5d09a06bfdf19c14006b8 is reconciled by a normal merge. All seven source/check/attribute file blobs already match current main. The three documentation conflicts retain the newer current-main records. The merged tree has no runtime or test changes. Historical verification retains its original source binding.

Unfinished diagnostic source 864ccf78118d5b656c97fced0ac6a25ae4fe7334 remains on recovery/reinstall-startup-tracing-20261013, read back with git ls-remote. Its two Python files were reviewed and preserved, but were not run. Do not call this diagnostic contribution complete or activate it through this preservation pass.

All inspected local source tips have a proved remote preservation route. Default-branch integration receives a separate exact remote SHA proof. Primary checkout and the linked main checkout are retained. Delete only adopted inactive non-default branches and linked checkouts whose current source tip is a proved ancestor of remote main and whose selected source and ignored-file inventory has no unique retained evidence. The restoration ledger records each actual deletion and retained item. No archive or cloud hash proof is claimed.

Next: verify the resulting main commit on the remote, keep the unfinished diagnostic branch, preserve local ignored evidence, and let the owner perform the reinstall. A remote source ref does not preserve ignored output, reflogs or private local configuration.
