<?php
@error_reporting(E_ERROR | E_PARSE);
@session_start();
include_once "inc/util.php";
include_once "inc/config.php";
include_once "inc/dbconnect.php";

// we haven't sent a partial status page yet
$pageStarted = false;

// connect to the database
$db = dbConnect();
if (!$db)
    errorExit("An error occurred connecting to the database.");

// make sure the cache table exists
createCacheTable($db);

// check for a "ping" request
if (isset($_REQUEST["ping"]))
{
    echo "OK \r\n";
    exit();
}

// Get the parameters.  Note that IFDB encodes any non-ASCII characters
// in the URL parameters in UTF-8, per Web standards for URL encoding.
// php currently uses ISO-8859-1 internally, so any strings we wish to
// use internally within this script must be translated, which can be
// done most easily via utf8_decode().  However, any strings to be passed
// on to the TADS interpreter must be kept in UTF-8, because the
// interpreter requires command-line parameters to be in UTF-8 when
// in web hosting mode.  We use the $gameTitle string internally only,
// so we translate it; the $username is to be passed on to TADS, so we
// don't translate it.  Other parameters should all be plain ASCII, so
// there's nothing else to worry about.
//
$gameid = get_req_data("tuid");
$gameTitle = utf8_decode(get_req_data("title")); // script use -> xlat to Latin1
$gameUrl = get_req_data("storyfile");
$storageSID = get_req_data("storagesid");
$qstorageSID = escapeshellarg($storageSID);
$username = get_req_data("username");  // NB - for TADS, so keep in UTF-8
$qusername = escapeshellargx($username);
$restore = get_req_data("restore");
$qrestore = escapeshellarg($restore);

// build a "try again" link
$tryAgain = "<a href=\""
            . $_SERVER['REQUEST_URI']
            . "\">Try again</a>";

// download the game file
$gameFile = fetchGame($gameUrl);

// build the t3run command line - start with the command name
$cmd = T3RUN_EXE . " ";

// add the basic command options
$cmd .= T3RUN_OPTS . " ";

// add the web host name - we want the interpreter to bind its web listener
// to the same network adapter that received the current request
$cmd .= T3RUN_OPT_WEBHOST . " " . T3RUN_OPT_WEBHOST_NAME . " ";

// add the game URL to the argument list
$cmd .= T3RUN_OPT_GAMEURL . " " . escapeshellarg($gameUrl) . " ";

// add the storage server ticket ID, if present
if ($storageSID)
    $cmd .= T3RUN_OPT_WEBSID . " $qstorageSID ";

// add the saved game to restore, if any
if ($restore)
    $cmd .= " -r $restore ";

// add the local copy of the game file that we downloaded (or found
// in the cache from a previous download)
$cmd .= $gameFile;

// add the game ID to the game's argument list
$cmd .= " -gameid=" . escapeshellarg($gameid);

// add the storage server session ID, if we have one
if ($storageSID)
    $cmd .= " -storagesid=$qstorageSID";

// add the username parameter, if we have one
if ($username)
    $cmd .= " -username=$qusername";

// Set up pipes for the child's stdin, stdout, and stderr.  Note that that
// we create the stderr pipe in "a" (append) mode - this is a hack to work
// around a bug in php on Windows.
$handles = array(0 => array("pipe", "r"),
                 1 => array("pipe", "w"),
                 2 => array("pipe", "a"));

// Add the "background" qualifiers to the command line, so that tads launches
// as a detached background process.  We want t3run to continue running after
// this request is finished, so we need to launch it as a separate process,
// not as a child process that we wait for.
$sysname = php_uname("s");
if (preg_match("/^windows/i", $sysname))
{
    // Windows - detach using START, plus /B for no new console window
    // (we don't want a window since this is a faceless background app)
    $cmd = "start /b $cmd";
}
else
{
    // otherwise assume Unix/Linux - add "&" to the end of the command line,
    // to detach the process, and redirect stderr to stdout
    $cmd .= " 2>&1 &";
}

// open the process
$proc = proc_open($cmd, $handles, $pipes, null, null);
$reply = array();
if (is_resource($proc))
{
    // We successfully launched the interpreter process.  The interpreter
    // will fetch the game file and start executing it.  If this is a
    // valid web-enabled game, the game will start an HTTP listener, and
    // call connectWebUI().  That function will write the URL to the game's
    // startup page (as served by the game's internal HTTP server) to
    // stdout - which we can read as our pipe #1.  We complete the
    // connection with the client by replying to the launch request with
    // a redirect page pointing to the game's startup page.  This tells
    // the client browser to connect to the game, and the game's internal
    // HTTP server takes it from there.

    // explicitly close the child's stdin, in case we've launched a non-web
    // interactive game that's waiting for keyboard input - it won't get
    // any, so force an EOF immediately so it won't wait forever
    fclose($pipes[0]);

    // get the process identification and ohter information
    $procInfo = proc_get_status($proc);
    $pid = $procInfo["pid"];

    // read text the game writes to stdout
    $fpin = $pipes[1];
    for (;;)
    {
        // read from stdin - EOF means that the child has terminated, so
        // stop trying to read from it in that case
        $txt = fgets($fpin);
        if ($txt === false)
            break;

        // remove newlines
        $txt = preg_replace("/[\r\n]/", "", $txt);

        // add it to our reply list
        $reply[] = htmlspecialchars($txt);

        // check for our connection line
        if (preg_match("/^connectWebUI:(.*)$/", $txt, $m))
        {
            // This is the connection information - send back the
            // redirect page.  Get the connect address as indicated
            // by the game in the connectWebUI: message.
            $href = $m[1];

            // If we're behind a NAT firewall, we told the game to bind
            // to our numerical IP port, since that's the address that
            // we see locally on our ethernet port.  Because of the NAT
            // layer, though, the client sees a different address - the
            // local IP address is hidden behind the NAT router, so the
            // client knows us by a different address.  In this case,
            // substitute the server address that the client sent us
            // for the numerical IP binding address.  We know the client
            // knows us by that address because that's how they reached
            // us in the first place.
            if (USING_NAT)
                $href = preg_replace("/^(http:\/\/)?(\d+\.\d+\.\d+\.\d+):/",
                                     "http://" . $_SERVER["SERVER_NAME"] . ":",
                                     $href);

            $urlpar = preg_replace("/\"/", "%22", $href);
            redirectPage($href, "Loading Game",
                         "The game is loading. If your browser doesn't "
                         . "automatically display the game within a few "
                         . "moments, <a href=\"$urlpar\">click here</a>.");

            // close our process and pipe handles
            proc_close($proc);
            fclose($pipes[1]);
            fclose($pipes[2]);

            // clean up the cache
            cleanCache();

            // send the connection information to IFDB
            if ($storageSID)
            {
                $ifdb = IFDB_ROOT;
                $uurl = urlencode($urlpar);
                $ustorageSID = urlencode($storageSID);
                list($hdrs, $body) = x_http_get(
                    "$ifdb/t3register?session=$ustorageSID&addr=$uurl",
                    null, $errInfo, null);
            }

            // our work is done
            exit();
        }
    }

    // If we get this far, the child terminated without sending us the
    // connection code, which probably means that an error occurred
    // trying to start the program.  Show an error (including the
    // interpreter output, in case that's helpful to explain why the
    // program exited early) and terminate.
    proc_close($proc);
    errorExit("An error occurred starting the game.",
              "The game displayed the following message(s) before "
              . "it exited:</b><br><div class=\"errOutput\"><pre>"
              . implode("\r\n", $reply)
              . "</pre></div>"

              . "Note that the game must be designed for Web play to "
              . "run properly with this service. Standard TADS games "
              . "won't work on this system.");
}
else
{
    errorExit("An error occurred starting the game.",
              "Error details: the server was unable to start the "
              . "TADS interpreter. This could be because the "
              . "server is already running too many other programs, "
              . "or could be due to a configuration problem on the "
              . "server.");
}

function redirectPage($url, $title, $msg)
{
    global $pageStarted;

    if ($pageStarted)
    {
        // We've already send back a partial status page, so we can't
        // send a redirect.  Instead, reset the page and write a little
        // script to do the navigation.
        showPage($title,
                 $msg
                 . "\r\n<script type=\"text/javascript\">\r\n"
                 . "window.location = \""
                 . preg_replace("/\"/", "%22", $url)
                 . "\";\r\n"
                 . "</script>\r\n\r\n");
    }
    else
    {
        // we haven't written any output yet, so send a simple
        // HTTP 301 redirect
        header("HTTP/1.1 301 Moved Permanently");
        header("Content-type: text/html");
        header("Location: $url");
        showPage($title, $msg);
    }
}

function fetchGame($url)
{
    global $db, $gameTitle, $hurl;

    if (!$url)
        errorExit("The TADS interpreter can't run this game, because "
                  . "no story file was specified in the request. "
                  . "You might want to contact the maintainer of "
                  . "the page where you found the launch link.");

    // if the game URL doesn't look like an HTTP URL, abort
    if (!preg_match("#^(http)://#", $url))
        errorExit("The TADS interpreter can't run this game, because "
                  . "the story file path is invalid. You might want "
                  . "to contact the maintainer of the page where you "
                  . "found the launch link.",

                  "Story file location: $hurl<br>"
                  . "This doesn't appear to be a valid HTTP URL.  A "
                  . "Web address with an \"http://\" prefix is required.");

    // check to see if the URL is in our cache
    $filename = $filepath = false;
    $hurl = htmlspecialchars($url);
    $stmt = $db->prepare(
        "select filename, lastmod, contlen
         from dlcache
         where url=:url");
    $stmt->bindValue(':url', $url, PDO::PARAM_STR);
    $stmt->execute();

    // if that turned up, fetch the row
    if ($res = $stmt->fetch(PDO::FETCH_NUM))
        list($filename, $lastmod, $contlen) = $res;

    // if there's a file, look for it in the cache directory
    if ($filename)
        $filepath = T3_CACHE_DIR . "/$filename";

    // If we found a record for the URL in the db, make sure the file is
    // still in our cache.  If we're configured to use the system temp
    // directory, it might have been deleted by routine system temp
    // cleanup, which won't have updated the database.
    if ($filepath && !file_exists($filepath))
    {
        // forget about the db row
        $filename = $filepath = false;
    }

    // If we found an existing copy of the file, do an HTTP HEAD on the
    // URL to check for updates.
    if ($filename)
    {
        // do a HEAD on the source file
        $hdrs = x_http_head($url, $errInfo);
        if ($errInfo)
        {
            errorExit("A network error occurred retrieving the story file.",
                      "Story file location: $hurl<br>"
                      . "Error: unable to retrieve file information "
                      . "from remote server (HEAD request failed "
                      . "with network error {$errInfo[0]}: {$errInfo[1]}");
        }

        // the mod date and file size must exactly match, or we have to
        // assume that the file has been updated since we cached it
        if ($hdrs["last-modified"] != $lastmod
            || $hdrs["content-length"] != $contlen)
        {
            // it's been modified - replace the cached copy
            $filename = $filepath = false;
        }
    }

    // If we still like our cached file after all of that, simply return
    // the cached file.
    if ($filepath)
    {
        // mark the file as recently used in the database record
        $stmt = $db->prepare(
            "update dlcache
             set lastused = CURRENT_TIMESTAMP
             where url=:url");
        $stmt->bindValue(':url', $url, PDO::PARAM_STR);
        $stmt->execute();

        // return the file path
        return $filepath;
    }

    // We either don't have a cached copy of the file, or we have a copy
    // but it's out of date.  Delete any existing row for the file in
    // the database, since if we do have a cached copy we've decided
    // we need to replace it.
    $stmt = $db->prepare(
        "delete from dlcache where url=:url");
    $stmt->bindValue(':url', $url, PDO::PARAM_STR);
    $stmt->execute();

    // Create a temp filename.  Roll up a random md5 string to use as
    // the temp name.
    $filename = md5_rand($url) . ".t3";
    $filepath = T3_CACHE_DIR . "/$filename";

    // send the status page while we're doing the download, since this
    // could take a while
    startPage("Loading story file");
    ?>
    <h1><?php echo htmlspecialchars($gameTitle); ?></h1>

    TADS is loading the story file from the network server
    (<?php echo htmlspecialchars($url); ?>).  Please stand by...

    <div id="xferStatus">Status: downloading</div>
    <script type="text/javascript">
    function updateStatus(msg)
    {
        document.getElementById("xferStatus").innerHTML = msg;
    }
    </script>
    <?php
    echo "\r\n";
    flush();
    ob_flush();

    // download the file
    $hdrs = x_http_get($url, $filepath, $errInfo, "getStatusCallback");
    if ($hdrs
        && preg_match("#HTTP/[0-9.]+\s+(([0-9]+)\s+(.*)+)$#",
                      $hdrs["http status"], $m))
    {
        $httpStat = intval($m[2]);
        $httpMsg = $m[1];
        if ($httpStat != 200)
        {
            unlink($filepath);
            errorExit("A network error occurred retrieving the story file.",
                      "Story file location: $hurl<br>"
                      . "HTTP error: $httpMsg");
        }
    }
    else if ($errInfo)
    {
        unlink($filepath);
        errorExit("A network error occurred retrieving the story file.",
                  "Story file location: $hurl<br>"
                  . "Network error {$errInfo[0]}: {$errInfo[1]}");
    }

    updateStatus("Status: download complete");

    $lastmod = $hdrs["last-modified"];
    $contlen = (isset($hdrs["content-length"])
                ? $hdrs["content-length"]
                : filesize($filepath));


    // Validate the file: make sure it looks like a valid .t3 file,
    // at least as far as the signature goes.  If it's not, delete it.
    // This provides a tiny bit of security, by somewhat limiting what
    // we're willing to store in our local server file system.  At
    // the very least, a file with a T3 image signature can't be a
    // native executable, a shell script, or a common format like
    // ZIP for which the system might have built-in support.  It could
    // still contain other dangerous data after the signature, but a
    // file with the T3 signature is pretty much guaranteed not to be
    // directly executable on its own: a hacker would need to have
    // some other hooks in our system to make mischief with a T3, but
    // if they have those hooks anyway, they don't need the T3 to make
    // mischief.  This should render anything we download pretty inert.
    $isT3 = false;
    $fp = fopen($filepath, "r");
    if ($fp)
    {
        // read and check the signature
        $sig = fread($fp, 32);
        if (strncmp($sig, "T3-image\015\012\032", 11) == 0)
            $isT3 = true;

        // done with the file
        fclose($fp);
    }

    // if it's not a T3, fail
    if (!$isT3)
    {
        // delete the file and abort
//        unlink($filepath);
        errorExit("The story file doesn't appear to be a valid "
                  . "TADS game file.",
                  "Story file location: $hurl<br>"
                  . "This file isn't a valid .t3 compiled game file. "
                  . "Possible causes:<ul>"
                  . "<li>the original Web link you clicked to launch the "
                  .   "game is broken"
                  . "<li>the story file on the server is corrupted"
                  . "<li>the file was uploaded with a compressed format "
                  .   "such as ZIP, rather than as a plain .t3 file"
                  . "<li>the file was garbled while being transferred "
                  .   "across the network"
                  . "</ul>You might try again, in case the problem was a "
                  . "random network glitch or other temporary condition. "
                  . "If the problem persists, you should contact the "
                  . "person who maintains the page where you found the "
                  . "launch link.");
    }

    // everything looks good - add the cache record to the database
    $stmt = $db->prepare(
        "insert into dlcache
         (url, filename, lastmod, contlen, lastused)
         values (:url, :filename, :lastmod, :contlen, CURRENT_TIMESTAMP)");
    $stmt->bindValue(':url', $url, PDO::PARAM_STR);
    $stmt->bindValue(':filename', $filename, PDO::PARAM_STR);
    $stmt->bindValue(':lastmod', $lastmod, PDO::PARAM_STR);
    $stmt->bindValue(':contlen', $contlen, PDO::PARAM_STR);
    $stmt->execute();

    // note that we're starting the game
    updateStatus("Status: download complete; starting game");

    // return the file path
    return $filepath;
}

function updateStatus($msg)
{
    echo "<script type=\"text/javascript\">\r\n"
        . "updateStatus(\""
        . preg_replace("/\"/", "\"+String.fromCharCode(34)+\"", $msg)
        . "\");\r\n"
        . "</script>\r\n";

    flush();
    ob_flush();
}

function getStatusCallback($stat)
{
    if ($stat == "read")
    {
        $hdrs = func_get_arg(1);
        $bytes = func_get_arg(2);

        if (isset($hdrs["content-length"]))
        {
            $clen = $hdrs["content-length"];
            updateStatus("Status: downloading ("
                         . intval(100.0 * $bytes / $clen) . "%)");
        }
        else
        {
            if ($bytes > 1000000)
                $bytes = round($bytes/1048576.0, 1) . " MB";
            else if ($bytes > 1000)
                $bytes = round($bytes / 1024.0, 2) . "KB";
            else
                $bytes .= " bytes";

            updateStatus("Status: downloading (received $bytes)");
        }
    }
}

// Create the cache table if it doesn't already exist.  This will
// fail harmlessly if the table is already there.
function createCacheTable($db)
{
    // create the download cache table if missing
    $stmt = $db->prepare("CREATE TABLE `dlcache` (
        `url` varchar(4096) default NULL,
        `filename` varchar(255) default NULL,
        `lastmod` varchar(128) default NULL,
        `contlen` int(20) default NULL,
        `lastused` datetime default NULL)");

    if ($stmt)
        $stmt->execute();
}


// Clean out the cache.  Checks the amount of space currently used
// for cached files, and deletes old files if we're over the limit.
//
// We won't remove files access very recently (within the past few
// minutes).  This will automatically exempt the file for the current
// request from deletion.  Files accessed very recently could be in
// use by other requests that haven't completed the launch process
// yet, so we don't want to interfere with those requests by deleting
// their working files.
function cleanCache()
{
    global $db;
    if (!$db)
        return;

    // Query the database's list of cached files.  Sort the files by
    // access time, from oldest to newest.  This puts the list in
    // order of deletion if we do decide to prune the cache.
    //
    // Exclude files that have been accessed very recently.  This
    // prevents us from deleting the file we fetched for the current
    // request, as well as files that might be actively in use by
    // other incomplete requests for other clients.
    $stmt = $db->prepare(
        "select url, filename
         from dlcache
         where DATE(lastused) < DATE(CURRENT_TIMESTAMP)
         order by lastused");
    $stmt->execute();

    // fetch the results
    $cacheSize = 0;
    $files = array();
    while ($res = $stmt->fetch(PDO::FETCH_NUM))
    {
        // fetch the row
        list($url, $filename) = $res;
        $filepath = T3_CACHE_DIR . "/$filename";

        // if the file still exists, include it in the list
        if (file_exists($filepath))
        {
            // get the actual file size, and add it to the running total
            $fsize = filesize($filepath);
            $cacheSize += $fsize;

            // include this file in the active file list
            $files[] = array($url, $filepath, $fsize);
        }
    }

    // Prune the cache.  The list is arranged from oldest to newest, so
    // work through the list and delete old files until the total size
    // is below the cache limit, or we run out of files.
    for ($i = 0, $cnt = count($files) ;
         $i < $cnt && $cacheSize > T3_CACHE_MAX ; $i++)
    {
        // get this file's information from the list
        list($url, $filepath, $fsize) = $files[$i];

        // remove the database record for the file
        $stmt = $db->prepare(
            "delete from dlcache where url = :url");
        $stmt->bindValue(':url', $url, PDO::PARAM_STR);
        $stmt->execute();

        // remove the file
        unlink($filepath);

        // deduct the file's space from the cache total
        $cacheSize -= $fsize;
    }
}

function errorExit($msg, $details = "")
{
    // show the error page
    errorPage($msg, $details);

    // clean the cache while we're here
    cleanCache();

    // terminate
    exit();
}

function errorPage($msg, $details = "")
{
    global $gameid, $gameUrl, $gameTitle, $tryAgain;

    if ($gameTitle)
        $msg = "<h1>" . htmlspecialchars($gameTitle) . "</h1>" . $msg;
    else
        $msg = "<h1>Error</h1>" . $msg;


    $msg .= "<div class=\"tryAgain\">$tryAgain</div>";

    if ($details)
    {
        $msg .= "<div class=\"errDetails\">"
                . "<a href=\"#\" onclick=\"javascript:"
                .   "this.nextSibling.style.display='block';"
                .   "this.style.display='none';"
                .   "return false;\">Show details</a>"
                . "<div style=\"display:none;\"><b>Error details:</b> "
                . "$details</div>"
                . "</div>";
    }

    $ifdb = IFDB_ROOT;
    $msg .= "<div class=\"navFooter\">";
    if ($gameid) {
        $purl = urlencode($gameUrl);
        $msg .= "<a href=\"$ifdb/t3run?id=$gameid&storyfile=$purl\">"
                . "Return to the game's launch page</a> | "
                . "<a href=\"$ifdb/viewgame?id=$gameid\">"
                . "Game's main page</a> | ";
    }
    $msg .= "<a href=\"$ifdb/\">IFDB home page</a>";

    showPage("Error launching game", $msg);
}

function showPage($title, $msg)
{
    startPage($title);
    echo $msg;
    endPage();
}

function startPage($title)
{
    global $pageStarted;

    if ($pageStarted)
    {

    ?>

    <script type="text/javascript">
       resetPage(<?php
         echo '"' . preg_replace("/\"/", "\"+String.fromCharCode(34)+\"")
              . '"' ?>);
    </script>

    <?php

    }
    else
    {
        $pageStarted = true;

    ?>
<html>
<head>
   <title><?php echo $title ?></title>
   <link rel="stylesheet" href="t3launch.css">
</head>
<body>
<div class="topbar">
</div>
<script type="text/javascript">
function resetPage(title)
{
    document.getElementById("mainDiv").innerHTML = "";
    document.title = title;
}
</script>
<div class="main" id="mainDiv">
    <?php

    }
}

function endPage()
{
   ?>

</div>
</body>
</html>

   <?php
   flush();
   ob_flush();
}

?>