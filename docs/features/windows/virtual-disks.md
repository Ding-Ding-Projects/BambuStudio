# Virtual disks

**Surface:** File > Virtual disks, the command palette (`Ctrl+Shift+F`, type
"Virtual") and the Every feature page →
`src/slic3r/GUI/VirtualDisks/VirtualDisksPanel.{hpp,cpp}`. Reading disk files
lives in `DiskImage.{hpp,cpp}`, the limits and the steps of every operation in
`DiskPlan.{hpp,cpp}`, and what Windows is asked and how operations run in
`DiskSystem.{hpp,cpp}`, all in the same folder.

Every VHD, VHDX, VHD Set and ISO you add, read from the files themselves, with
every operation Windows offers. Nothing here touches a disk you did not choose,
and nothing needs Hyper-V.

## The list

Add disks with **Add a disk file...**, **Add a folder...** (every disk file in
it, three folders deep), or by dropping `.vhd`, `.vhdx`, `.avhdx`, `.vhds` or
`.iso` files anywhere on the page. **Remove from the list** forgets a disk; the
file stays where it is.

| Column | Shows |
| --- | --- |
| Name | The file name; a child sits under its parent with an arrow |
| Folder | Where the file is |
| Format and type | VHD, VHDX, VHD Set or ISO; fixed, dynamic, differencing or read-only |
| Size | The size the disk shows to Windows |
| On disk | What the file takes on its drive |
| Attached | No, Yes, or where its volume is mounted |
| Chain test | Passed, Protected (a parent with children), Parent missing, Parent changed, Unreadable, Same disk ID, or Not a chain |

**All**, **Attached**, **In chains** and **Problems** filter the list, each with
its count, and the search finds a name, path, label or ID. **Read again** reads
every file and asks Windows again.

## Reading a disk

The details beside the list come from the file itself:

- **VHD:** the footer at the end of the file (its checksum is checked), and for
  dynamic and differencing disks the dynamic header: block size, the parent's
  unique ID, name and stored paths.
- **VHDX:** both headers (the one that passes its CRC-32C check with the higher
  sequence number is current), the region table, and the metadata: block size,
  logical and physical sector sizes, virtual size, the virtual disk ID, and for
  a child the parent locator.
- **ISO:** the volume label and size from the primary volume descriptor.
- **Inside the disk:** the partition table, when the disk's first block is in
  this file: GPT with its disk GUID, MBR with its signature, or raw.
- **VHD Set:** the file is recognised; its checkpoints are read by Windows with
  the Hyper-V module, so they are not shown here.

A disk attached read-write cannot be read while Windows holds it; the page keeps
showing what it read last.

## Chains and clashes

A differencing disk (a child) names its parent by path and by identity. The
parent is found by the stored absolute path, then the relative path from the
child's folder, then the file name in the child's folder. The chain test then
checks the identity: a VHDX child records its parent's data write GUID, a VHD
child its parent's unique ID. If the parent was written to after the child was
made, the child shows **Parent changed**; if no listed file matches,
**Parent missing**.

Two disks whose partition tables carry the same disk ID **clash**: Windows keeps
the second one offline while the first is attached. Both show **Same disk ID**,
and **New disk ID...** ends it. A child and its parent share their table by
design and never count as a clash.

## Operations

Every operation is planned first and runs with administrator permission:
Windows asks once, and the step that is running shows at the bottom with
**Cancel**, which stops at the next step. If you cancel the administrator
prompt, nothing changes and the operation is recorded as incomplete, never as
done.

| Operation | What happens | Route |
| --- | --- | --- |
| Attach, Attach read-only | Attaches the disk; Windows mounts its volumes | Storage module |
| Detach | Detaches it, after asking | Storage module |
| Create... | See below | VirtDisk API, Storage module |
| Grow... | Grows the file; while attached, extends its last volume into the new space in the same run | VirtDisk API, Storage module |
| Compact... | Full scans for zero blocks and gives back unused ones; Quick gives back unused blocks without scanning. An attached disk is reattached read-only for the run and put back as it was | VirtDisk API |
| Make child... | A new differencing disk on this one | VirtDisk API |
| Merge into parent... | Behind the two-key confirmation, which names the parent and every other child that stops working. A verified copy of the parent (`Name (copy).vhdx`, checked by SHA-256) is made first | VirtDisk API |
| Reconnect parent... | Points a child at its parent's new place, after checking the chosen file is that parent | VirtDisk API |
| New disk ID... | A new GPT disk GUID or MBR signature; the disk is attached without a letter for the change and detached after | Storage module |
| Quick pack... | See below | VirtDisk API, Storage module, Robocopy |

Grow works on a disk that is neither a child nor a parent: growing a parent
would break its children. Compact works on dynamic and differencing disks.
Both need the disk's file to be readable. Shrinking is not built yet.

**Copy the steps...** (also on right-click in the list) copies any operation for
the selected disk as a PowerShell script, or as a DiskPart script where DiskPart
can do the same. A copied PowerShell script defines the helpers its steps use,
so it runs as pasted in Windows PowerShell started as administrator.

## Create

Start from **Portable data disk** (VHDX, dynamic, GPT, NTFS), **Dev Drive**
(ReFS, at least 50 GB, formatted as a Dev Drive) or **Disk of my own**, then
choose:

- **Format:** VHDX or VHD. **Type:** dynamic (takes only what it holds) or fixed
  (takes everything at once).
- **Size** in MB, GB or TB. A VHDX holds 3 MB to 64 TB, a VHD up to 2,040 GB.
- **Block:** Windows' default, or 1, 8, 32 or 256 MB for a VHDX (a VHD uses 512
  KB or 2 MB). **Sectors:** 512 or 4096 logical, 512 or 4096 physical.
- **Initialize:** GPT, MBR (at most 2 TB) or leave it raw. **File system:** NTFS,
  ReFS, exFAT or FAT32 (Windows formats FAT32 up to 32 GB), with a label that
  fits it (32, 15 or 11 characters). **Drive letter:** the next free one or one
  you choose. **Quick format.**
- **Where it goes** (never over an existing file) and **Attach after creating**.

Every limit is checked as you type, with notes such as the free space on the
drive. **Same steps as a script** shows the PowerShell or DiskPart steps, with
**Copy**. DiskPart uses its own block size; the VirtDisk API honours the one you
chose.

## Quick pack

Choose a folder: it is packed into a new dynamic VHDX beside it, sized to fit
(the files, about 6 KB per file for the file system's own records, 2 % and 64
MB). If `Photos.vhdx` exists the new disk is `Photos (1).vhdx`; nothing is
overwritten. The folder tree, times, attributes and alternate data streams (web
marks included) are copied by Robocopy, junctions are not followed, and every
file is then checked by SHA-256 against its source. If a file grows while it is
copied and the disk runs out of room, the disk grows by a quarter and the copy
continues. **Cancel** detaches and removes only the new disk; your files are
never moved or changed. The new disk is detached at the end.

## History

**History** lists every operation, the route that did it and how it ended
(Done, Incomplete or Failed), with the size before and after for Compact, the
file count for Quick pack and the new ID for New disk ID. The last 200 are kept.

## Stored files

| File | Holds |
| --- | --- |
| `virtual-disks.json` in the data folder | The disks and folders you added, and the history |
| `virtual-disks\` in the data folder | Each operation's script while it runs; removed when it ends |

## Without Windows

The page says that virtual disks need Windows. Disk files can still be added,
read and checked; nothing can be attached or changed.

## Checks

- `tests/virtual_disks.test.mjs` compiles `tests/virtual_disks_test.cpp`: VHD,
  VHDX and ISO images built byte by byte and read back (the newer VHDX header
  chosen, damaged checksums reported), the partition table inside, chains found,
  intact, changed and missing, clashing disk IDs, every limit, every operation's
  PowerShell and DiskPart steps, Quick pack sizing and naming, and the library
  file. Its negative build drops the size limits and calls a parent and its
  child a clash, and must fail.
- `tests/virtual_disks_windows.test.mjs` runs on a hosted Windows machine
  (`.github/workflows/virtual-disks-windows.yml`) with real disks through the
  app's own code: create a VHDX with an 8 MB block and 4096-byte physical
  sectors and read it back, make a child, write through it and merge it into
  the parent after a verified copy, grow detached and attached, compact, end a
  clash with a new disk ID, reconnect a moved parent, and Quick pack a folder
  whose web mark must survive.

## Not done yet

Shrinking, attaching at every sign-in, Retrim, Pretrimmed and Prezeroed
compacting, VHD Set checkpoints, native boot disks, Dev Drive filters, live
mirrors and change tracking are designed but not built. Turning off the
verified copy before a merge is not offered.
