# IANA Time Zone Database

    /tads3/tz/db/

This directory is in the TADS 3 source code distribution as a placeholder for the IANA timezone database file.
They're not included in the TADS distribution, but these public domain files are available from the IANA site at

- https://www.iana.org/time-zones

Download the zoneinfo database files and gunzip/untar them into this directory.
You can then run the TADS timezone compiler, [`tz.t`][tz.t], which should be in this directory's parent folder.
Run these commands in that parent folder to build the compiler and compile the database:

    t3make -f tz.t3m
    t3run -plain tz db vmtz.cpp

The `t3run tz` command will read the timezone database files from the `db` folder and generate the output file [`vmtz.cpp`][vmtz.cpp].
That file can then be copied into the TADS source tree in the [`tads3/derived`][tads3/derived] folder to replace the previous generated version.

<!-----------------------------------------------------------------------------
                               REFERENCE LINKS
------------------------------------------------------------------------------>

[tads3/derived]: ../../derived/ "Navigate to folder"
[tz.t]: ../tz.t "View source file"
[vmtz.cpp]: ../../vmtz.cpp "View source file"

<!-- EOF -->
