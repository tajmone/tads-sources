<?php
//
// TADS Web Player configuration.  This file must be customized for
// each installation
//

// ------------------------------------------------------------------------
// Database configuration.  The launch script needs a small database for
// keeping track of its download cache.  Each launch request will come
// with the URL of the .t3 file to run.  We download these files to a
// local temp directory for actual execution.  We use the database to keep
// track of these downloads, so that if we get another request for the
// same URL, we can use the previously downloaded copy rather than having
// to do a new download on every request.
//
// Our database usage is extremely lightweight (in terms of both load and
// required SQL feature set).  The underlying database can be either MySQL
// or SQLite3.

// MySQL login information - uncomment and fill in these parameters if
// you're using MySQL as the underlying database engine
//define("DB_ENGINE", "mysql");
//define("DB_HOST", "localhost");
//define("DB_USERNAME", "t3launch");
//define("DB_PASSWORD", "password");
//define("DB_SCHEMA", "t3launch");

// SQLite3 database path - uncomment and fill in this parameter if you're
// using SQLite3 as the database engine
//define("DB_ENGINE", "sqlite");
//define("DB_FILE", "/home/support/db/t3launch.db");



// ------------------------------------------------------------------------
// The t3run executable.  Enter this as the full absolute path to the
// program.  On Windows, use forward slashes, and start with X:/ (where
// (X is the drive letter).  On Unix, start with / (not ./ or ~/ other
// relative notation).

// typical setting for Windows
// define("T3RUN_EXE", "c:/program files/tads3/t3run");

// typical setting for Linux
// define("T3RUN_EXE", "/usr/local/bin/frob");

// ------------------------------------------------------------------------
// Command options for t3run.  t3run and FrobTADS (and potentially other
// packages) have different command syntax for option flags.  Select the
// option flag syntax suitable for your installation.

// For t3run, uncomment the following lines:
// define("T3RUN_OPTS", "-plain -s44 -ns00");
// define("T3RUN_OPT_WEBHOST", "-webhost");
// define("T3RUN_OPT_WEBSID", "-websid");
// define("T3RUN_OPT_GAMEURL", "-webimage");

// For FrobTADS, uncomment the following lines:
// define("T3RUN_OPTS", "--interface plain --safety-level 44 --net-safety-level 00 --no-pause");
// define("T3RUN_OPT_WEBHOST", "--webhost");
// define("T3RUN_OPT_WEBSID", "--websid");
// define("T3RUN_OPT_GAMEURL", "--webimage");

// ------------------------------------------------------------------------
// Are we behind a NAT firewall or router?  If so, our local IP address
// as we see it on our ethernet port is different from our public IP
// address as seen by clients.  We assume that we're using an ordinary
// server configuration with no NAT at our end.  Set this to 1 if the
// server is behind a NAT router.  (If your server is a PC on a home
// network connected to DSL, cable broadband, etc, and you have a
// broadband router box, you're probably using NAT.  Amazon EC2 servers
// are also connected to the public Internet through NAT routers.  In
// contrast, most commercial hosting services, including shared hosting
// and VPS providers, connect their servers directly to the public
// Internet with no NAT.)
define("USING_NAT", 0);

// Web server host name.  This is the address we pass to the game to
// use as its binding port for its HTTP lister.  By default, this is
// the same server that's handling the current request.  If we're
// behind a NAT firewall, however, we use the local server IP address
// instead, since our server name is presumably mapped to the public
// address we use outside the NAT, whereas for listener socket binding
// purposes we need the local private address instead.
if (USING_NAT)
    define("T3RUN_OPT_WEBHOST_NAME", $_SERVER['SERVER_ADDR']);
else
    define("T3RUN_OPT_WEBHOST_NAME", $_SERVER['SERVER_NAME']);

// ------------------------------------------------------------------------
// The IFDB URL root.  Do not include a trailing '/'.
define("IFDB_ROOT", "http://ifdb.tads.org");

// ------------------------------------------------------------------------
// .t3 download and caching directory.  This is the location where we
// store downloaded story files.  The web server must be able to write
// to this directory, and t3run must be able to read from it.  You can
// use the system temp file directory if desired.
//
// This directory serves as a cache for downloaded files.  We check
// for an existing copy of a file when we're about to download a
// new file, so that we only have to wait for a download on the first
// use of a given file.

// typical setting for Windows
// define("T3_CACHE_DIR", "c:/temp/");

// typical setting for Linux
// define("T3_CACHE_DIR", "/tmp");

// Maximum cache size, in bytes.  We'll periodically delete old files
// as needed to keep the cache size below this limit.
define("KILOBYTE", 1024);
define("MEGABYTE", 1024*KILOBYTE);
define("GIGABYTE", 1024*MEGABYTE);
define("T3_CACHE_MAX", 10*GIGABYTE);

?>