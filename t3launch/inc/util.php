<?php

// --------------------------------------------------------------------------
// get request data, stripping "magic quotes" as needed
//
function get_req_data($id)
{
    // get the raw value from the posted data
    $val = (isset($_POST[$id]) && $_POST[$id]) ? $_POST[$id] :
           (isset($_REQUEST[$id]) ? $_REQUEST[$id] : "");

    // if magic quotes are on, strip slashes
    if (get_magic_quotes_gpc())
        $val = stripslashes($val);

    // Opera has a bug (at least, it looks like a bug to me) that we need
    // to work around here.  If posted form data contain certain extended
    // characters, Opera will encode them as &#dddd; entities.  This is
    // problematic because it *doesn't* encode ampersands the same way,
    // so we can't be sure whether a &#dddd; sequence was originally a
    // special character, or is that literal sequence typed in by the
    // user.  As a workaround, we just flatly assume that all such
    // sequences are Opera encodings, since it seems so unlikely that
    // a user would type such a thing literally.  We only apply this for
    // curly quotes.
    if (is_opera()) {
        $val = str_replace(
            array('&#8220;', '&#8221;', '&#8216;', '&#8217;'),
            array("\223", "\224", "\221", "\222"),
            $val);
    }

    // return the result
    return $val;
}

// --------------------------------------------------------------------------
// are we on an iPod or iPhone?
//
function is_ipod_or_iphone()
{
    // get the browser ID string
    $b = $_SERVER['HTTP_USER_AGENT'];

    // check for the relevant strings
    return preg_match(
        "/^Mozilla\/[0-9.]+ \((iPhone|iPod|iPhone Simulator);/",
        $b, $match, 0, 0);
}

// is the browser Opera?
function is_opera()
{
    // get the browser ID string
    $ua = $_SERVER['HTTP_USER_AGENT'];

    // check for the telltale substring
    return stristr($ua, 'opera') !== false;
}

// ------------------------------------------------------------------------
//
// Generate a random MD5 string.  This concatenates the given "key" string
// with a number of system randomness sources and MD5-hashes the result.
//
function md5_rand($key)
{
    return md5(rand() . $key . mt_rand() . time() . microtime() . mt_rand());
}

// ------------------------------------------------------------------------
//
// Simple http HEAD.  Returns the headers as an array of value strings
// keyed by header names.  Returns false on failure.
//
function x_http_head($url, &$errInfo)
{
    // parse the URL into the domain and resource path
    preg_match("/^http:\/\/([-a-z0-9.]+)(\/.*$)/i", $url, $match);
    $addr = $match[1];
    $res = $match[2];

    // open the socket
    $errInfo = false;
    $fp = fsockopen($addr, 80, $errno, $errstr, 30);
    if (!$fp)
    {
        $errInfo = array($errno, $errstr);
        return false;
    }

    // send the HEAD request
    $req = "HEAD $res HTTP/1.1\r\n"
           . "Host: $addr\r\n"
           . "Resource: $res\r\n"
           . "Connection: Close\r\n\r\n";
    fwrite($fp, $req);

    // read the reply
    for ($msg = "" ; !feof($fp) ; $msg .= fgets($fp, 4096)) ;

    // done with the socket
    fclose($fp);

    // the reply should consist entirely of headers, so return what we got
    return parse_http_headers($msg);
}

// Simple http GET.  Saves the contents of the file in the given file
// stream, and returns the headers, as an array of name=>value pairs.
// If we're unable to establish a connection and read headers, returns
// false.
function x_http_get($url, $filename, &$errInfo, $cb = "")
{
    // no headers yet
    $hdrs = false;

    // parse the URL into the domain and resource path
    preg_match("/^http:\/\/([-a-z0-9.]+)(\/.*$)/i", $url, $match);
    $addr = $match[1];
    $res = $match[2];

    // open the socket
    $errInfo = false;
    $fp = fsockopen($addr, 80, $errno, $errstr, 30);
    if (!$fp)
    {
        if ($cb)
            $cb("error", "network error $errno: $errstr");
        $errInfo = array($errno, $errstr);
        return false;
    }

    if ($cb)
        $cb("open");

    // send the GET request
    $req = "GET $res HTTP/1.0\r\n"
           . "Host: $addr\r\n"
           . "Resource: $res\r\n"
           . "Connection: Close\r\n\r\n";
    fwrite($fp, $req);

    if ($cb)
        $cb("get");

    // read the headers
    $content = false;
    for ($msg = "" ; !feof($fp) ; )
    {
        // read the next chunk and append it to the buffer
        $chunk = fgets($fp, 1024);
        if ($chunk)
            $msg .= $chunk;

        // check for the end of the headers
        if (($idx = strpos($msg, "\r\n\r\n")) !== false)
        {
            // that's it for the headers - parse the headers
            $hdrs = parse_http_headers(substr($msg, 0, $idx));

            // the remainder of the buffer is the start of the contents
            $content = substr($msg, $idx + 4);

            // done scanning headers
            break;
        }
    }

    // if we didn't get any headers, it's a failure
    if (!$hdrs)
    {
        if ($cb)
            $cb("error", "No headers received from server");
        fclose($fp);
        return false;
    }

    // pass the headers to the callback, if provided
    if ($cb)
        $cb("headers", $hdrs);

    // if there's an output file, open it
    if ($filename)
    {
        // open the output file
        $fpout = fopen($filename, "w");
        if (!$fpout)
        {
            if ($cb)
                $cb("error", "Unable to open output file '"
                    . htmlspecialchars($filename) . "'");
            fclose($fp);
            return false;
        }

        // copy the initial content chunk to the file
        $bytesSoFar = strlen($content);
        fwrite($fpout, $content);
        if ($cb)
            $cb("read", $hdrs, $bytesSoFar);
    }

    // copy the remainder of the body to the file or to the in-memory string
    while (!feof($fp))
    {
        // read the next chunk from the socket
        $chunk = fread($fp, 4096);
        if (!$chunk)
            break;

        // write it to the file or in-memory string, as applicable
        if ($filename)
        {
            // add it to the file
            $bytesSoFar += strlen($chunk);
            fwrite($fpout, $chunk);
            if ($cb)
                $cb("read", $hdrs, $bytesSoFar);
        }
        else
        {
            // add it to the string
            $content .= $chunk;
        }
    }

    // done with the output file
    fclose($fpout);

    // done with the socket
    fclose($fp);

    // tell the callback we're done
    if ($cb)
        $cb("ok");

    // if there's no file, return the content string along with the headers;
    // otherwise just return the headers
    if ($filename)
        return $hdrs;
    else
        return array($hdrs, $content);
}

function parse_http_headers($msg)
{
    // break the headers into a list of lines
    $msg = explode("\r\n", $msg);

    // the first header is special - enter it under name "http status",
    // which can't conflict with any ordinary headers since it's not
    // a valid header name
    $hdrs = array("http status" => $msg[0]);

    // build the headers
    for ($i = 1, $cnt = count($msg) ; $i < $cnt ; $i++)
    {
        // split this item into name and value
        $h = explode(":", $msg[$i], 2);
        $name = strtolower(trim($h[0]));
        $val = trim($h[1]);

        // if this header is already in the output list, append the value
        // to the existing value with a comma; otherwise add the new value
        if (isset($hdrs[$name]))
            $hdrs[$name] .= ",$val";
        else
            $hdrs[$name] = $val;
    }

    // return the header array
    return $hdrs;
}

// Escape a command shell argument.  This is a replacement for the standard
// php escapeshellarg(), which doesn't handle non-ASCII characters in all
// configurations.  Our version does the same quoting for single and double
// quotes that the standard version does, and passes non-ASCII characters
// unchanged.  (We also strip out any control characters entirely, replacing
// any we find with '?'.)
function escapeshellargx($str)
{
    $sq = escapeshellarg("'");
    $qu = substr($sq, 0, 1);
    $sq = preg_replace("/^.(.*).$/", "\\1", $sq);
    $dq = preg_replace("/^.(.*).$/", "\\1", escapeshellarg('"'));
    $str = str_replace(array("'", '"'), array($sq, $dq), $str);
    $str = preg_replace("/[\\x00-\\x1F]/", "?", $str);
    return "$qu$str$qu";
}

?>