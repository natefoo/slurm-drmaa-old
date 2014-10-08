Documentation
=============

The manual for the DRMAA for Simple Linux Utility for Resource Management
(SLURM) is available at project wiki:
http://apps.man.poznan.pl/trac/slurm-drmaa/wiki

Limitations
===========

This will not work on a standard SLURM installation. slurm-drmaa needs access
to SLURM's `working_cluster_rec` global in `libslurmdb`, which is not
extern/public. To fix this you'll need to compile a new `libslurmdb.so`. My
lazy method for doing this is compiling it once, then editing
`src/db_api/version.map`, and compiling again. You can then point to the custom
`libslurmdb.so` using `LD_LIBRARY_PATH` or whatever method you prefer.

Multiple clusters specified in the `-M/--clusters` option are not supported at
this time.  The code to support this can probably be copied from SLURM's
`src/sbatch/mult_cluster.c`.

Because DRMAA does not provide a means for reporting back which cluster is
selected, I've chosen to modify the format of the Job ID returned by the submit
function. If the native specification does not contain `-M/--clusters`, Job IDs
are numeric as before. If `-M/--clusters` is passed, the Job ID is appended
with a `'.'`, followed by the cluster name to which the job was submitted. The
functions which check job state will also accept Job IDs in this format.
