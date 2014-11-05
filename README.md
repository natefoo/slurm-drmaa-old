Documentation
=============

The manual for the DRMAA for Simple Linux Utility for Resource Management
(SLURM) is available at project wiki:
http://apps.man.poznan.pl/trac/slurm-drmaa/wiki

Branches
========

- [master](../../tree/master) - slurm-drmaa 1.0.7 (from source tarball, not
  SVN) with mostly-complete Slurm multicluster `-M`/`--clusters` support (see
  **Limitations** below).
- [slurm-drmaa-1.2.0](../../tree/slurm-drmaa-1.2.0) - slurm-drmaa 1.2.0 from
  SVN, no modifications
- [slurm-drmaa-1.2.0-clusters](../../tree/slurm-drmaa-1.2.0-clusters) -
  slurm-drmaa 1.2.0 with the same cluster modifications as in the master
  branch.
- [slurm-drmaa-1.2.0-multisubmit](../../tree/slurm-drmaa-1.2.0-multisubmit) -
  slurm-drmaa 1.2.0 with the multicluster submission (i.e. `sbatch
  --clusters=cluster1,cluster2`) functionality (See **Limitations** below)

Limitations
===========

libslurmdb incompatibility
--------------------------

Multicluster support will not work on a standard Slurm installation prior to
Slurm 14.11.  slurm-drmaa needs access to Slurm's `working_cluster_rec` global
in `libslurmdb`, which was not extern/public in older versions. However, older
versions can be used if you compile a new `libslurmdb.so`. To do so, apply the
following patch to the Slurm source, reconfigure, and recompile:

```diff
--- src/db_api/Makefile.am.orig	2014-05-06 11:24:19.000000000 -0500
+++ src/db_api/Makefile.am	2014-10-10 11:48:12.730845279 -0500
@@ -95,6 +95,7 @@
 	(echo "{ global:";   \
 	 echo "   slurm_*;"; \
 	 echo "   slurmdb_*;"; \
+	 echo "   working_cluster_rec;"; \
 	 echo "  local: *;"; \
 	 echo "};") > $(VERSION_SCRIPT)
 
--- src/db_api/Makefile.in.orig	2014-05-06 11:24:19.000000000 -0500
+++ src/db_api/Makefile.in	2014-10-10 11:48:22.765938016 -0500
@@ -915,6 +915,7 @@
 	(echo "{ global:";   \
 	 echo "   slurm_*;"; \
 	 echo "   slurmdb_*;"; \
+	 echo "   working_cluster_rec;"; \
 	 echo "  local: *;"; \
 	 echo "};") > $(VERSION_SCRIPT)
```
 
In my instance I then copied the resulting `.libs/libslurdb*.so*` to
slurm-drmaa's lib directory and configured slurm-drmaa with
`LDFLAGS='-Wl,-rpath=/path/to/slurm-drmaa/lib' ./configure`, but putting
`libslurmdb` elsewhere and/or simply setting `$LD_LIBRARY_PATH` at application
runtime also work.

Note that root privileges are not required for this to work. The canonical copy
of libslurmdbd.so does not need to be modified, only the one that libdrmaa
links to at runtime.

Unimplemented features
----------------------

Multiple clusters specified in a single `-M`/`--clusters` option (e.g. `-M
cluster1,cluster2`) are not supported in the `master` and
`slurm-drmaa-1.2.0-clusters` branches.

However, support for it has been hacked in to the
`slurm-drmaa-1.2.0-multisubmit` branch. From the commit message:

This is fairly hacky because the functionality to do this properly is not
available via Slurm's public API. And:

1. I wasn't going to take the time to figure out the mess of private headers I
   had to extract from the slurm source and include here, so a copy of the
   Slurm source is required at compile time.
2. I have about 1 hours' worth of experience with autoconf/automake at this
   point, so the changes I made there are crude.
3. I'm not going to put a lot of effort into making this nicer since the Slurm
   development team has put implementing the features necessary for doing this
   properly on their roadmap for Slurm 15.08:
   http://bugs.schedmd.com/show_bug.cgi?id=1234

That said, once compiled, it will work with a standard Slurm 14.11 (or earlier
versions with the `working_cluster_rec` patch).


Potential Job ID incompatibilites
---------------------------------

Because DRMAA does not provide a means for reporting back which cluster is
selected, I've chosen to modify the format of the Job ID returned by the submit
function. If the native specification does not contain `-M`/`--clusters`, Job
IDs are numeric as before. If `-M/--clusters` is passed, the Job ID is appended
with a `'.'`, followed by the cluster name to which the job was submitted (e.g.
`42.cluster1`). The functions which check job state and perform job controls
will also accept Job IDs in this format.
