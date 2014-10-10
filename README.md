Documentation
=============

The manual for the DRMAA for Simple Linux Utility for Resource Management
(SLURM) is available at project wiki:
http://apps.man.poznan.pl/trac/slurm-drmaa/wiki

Modifications
=============

Current changes to this fork from the upstream (PSNC) version are:

- Added support for Slurm's multicluster `-M`/`--clusters` native options.

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

Unimplemented features
----------------------

Multiple clusters specified in a single `-M`/`--clusters` option (e.g. `-M
cluster1,cluster2`) are not supported at this time.  The code to support this
can probably be copied from Slurm's `src/sbatch/mult_cluster.c`.

Potential Job ID incompatibilites
---------------------------------

Because DRMAA does not provide a means for reporting back which cluster is
selected, I've chosen to modify the format of the Job ID returned by the submit
function. If the native specification does not contain `-M`/`--clusters`, Job
IDs are numeric as before. If `-M/--clusters` is passed, the Job ID is appended
with a `'.'`, followed by the cluster name to which the job was submitted (e.g.
`42.cluster1`). The functions which check job state and perform job controls
will also accept Job IDs in this format.
