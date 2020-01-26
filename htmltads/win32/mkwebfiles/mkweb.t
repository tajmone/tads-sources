#charset "us-ascii"

/*
 *   Copyright 2006 Michael J. Roberts.  Permission is hereby granted
 *   for anyone to use, modify, and distribute this file for any
 *   purpose.
 *
 *   This is the default Web Page generator program for TADS
 *   Workbench.  Workbench invokes this program, passing in a list of
 *   command-line arguments to supply the Web Page build settings. The
 *   settings are given as name/value pairs; the names that Workbench
 *   passes in are as follows:
 *
 *   out - the output directory name (always included)
 *
 *   info - the GameInfo.txt filename (always included)
 *
 *   zipfile - the release ZIP package file (only one of zipfile or
 *   gamefile will be included, according to the build settings)
 *
 *   gamefile - the compiled-for-release (.t3) game file (only one of
 *   zipfile or gamefile will be included)
 *
 *   sourcefile - the source ZIP package file (included only if the
 *   build settings so specify)
 *
 *   setupfile - the Windows SETUP package .exe file (included only if
 *   the build settings so specify)
 *
 *   tadsver - the major TADS version number - set to 2 for TADS 2, 3
 *   for TADS 3 (always included)
 *
 *   coverart - the Cover Art JPEG or PNG file (included only if a cover
 *   art file is defined for the project)
 *
 *   extra.file - the filename of an extra file (included once for each
 *   Web Page Extra file defined in the project)
 *
 *   extra.title - the hyperlink display title of an extra file
 *   (included once for each Web Page Extra file defined in the
 *   project; the order of these items is the same as the order of the
 *   extra.file items, so the nth extra.title value gives the title for
 *   the nth extra.file item; if no title is set in the project for a
 *   Web Page Extra file, the file's extra.title is simply passed as an
 *   empty string)
 *
 *
 *   Note that this program contains some Windows-specific code; the
 *   non-portable code is identified as such in comments.  This program
 *   is intended for use with TADS Workbench on Windows, so we're not
 *   especially concerned with making it portable.
 */

#include <tads.h>
#include <lookup.h>
#include <file.h>
#include <bignum.h>

/* ------------------------------------------------------------------------ */
/*
 *   Write the main index page.
 *
 *   This routine contains all of the logic that actually lays out the
 *   web page.  If you want to customize the web page layout, you should
 *   be able to do what you want simply by replacing this function -
 *   the rest of this file can be treated as a library of service
 *   routines that you can call but shouldn't need to change.
 *
 *   When this routine is called, global.valTab is set up with the
 *   name/value tab, which is populated with all of the command-line
 *   arguments and all of the GameInfo.txt values; the main output file
 *   is open, and can be referenced via the File object in
 *   global.outFile; and finally, the default output function is set to
 *   write to the output file, so you can write the output file using
 *   the double-quoted string syntax that would normally write to the
 *   console.
 */
generatePage()
{
    local v;

    /* write the <head> section */
    "<html>
    \n<head>
    \n<title><<getDef('name', 'Untitled').htmlify()>></title>
    \n<link rel=stylesheet href=\"storyweb.css\">
    \n</head>";

    /* copy the style sheet from our program directory */
    copySysFile('storyweb.css');

    /* start the body with the title of the game */
    "\n<body>
    \n<h1><<getDef('name', 'Untitled').htmlify()>></h1>";

    /*
     *   start the main table: left column for the game statistics and
     *   description, right column for the download links
     */
    "<table width=100%><tr valign=top>
    \n<td>
    \n<p><table><tr valign=top>";

    /* if there's cover art, add it here */
    if ((v = getDef('coverart', nil)) != nil)
    {
        /* copy it to the output directory */
        copyProjectFile(v);

        /* add it to the page */
        "<td class=coverart>
        <a href=\"<<winGetRootName(v)>>\">
        <img width=175 height=175 src=\"<<winGetRootName(v)>>\" border=0>
        </a>\n";
    }

    "<td>\n";

    /* add the headline, if present */
    if ((v = getDef('headline', nil)) != nil)
        "<b><<v.htmlify()>></b><br>\n";

    /* add the HTML byline (or plain text byline, if no HTML version) */
    if ((v = getDef('htmlbyline', nil)) != nil)
        "<b><<v>></b><br>\n";
    else if ((v = getDef('byline', nil)) != nil)
        "<b><<v.htmlify()>></b><br>\n";
    else
        "<b>Anonymous</b><br>\n";

    /* add the various statistics, as found */
    "<br><span class=details>\n";
    if ((v = getDef('series', nil)) != nil)
        "Part of <i><<v.htmlify()>></i><br>\n";
    if ((v = getDef('genre', nil)) != nil)
        "<<v.htmlify()>><br>\n";
    if ((v = getDef('releasedate', nil)) != nil)
        "Released <<formatDate(v)>><br>\n";
    if ((v = getDef('forgiveness', nil)) != nil)
        "Forgiveness Rating: <<v.htmlify()>><br>\n";

    /*
     *   add the link to the IFDB page - we can link by IFID, so there's no
     *   need to know IFDB's internal identifier for the game
     */
    if ((v = getDef('ifid', nil)) != nil)
    {
        /*
         *   if there's more than one IFID, pull out the first - any of our
         *   IFIDs will take us to the game's page, so it doesn't matter
         *   which one we use
         */
        local i;
        if ((i = v.find(',')) != nil)
            v = v.substr(1, i-1);

        /* remove any spaces from the IFID */
        v = v.findReplace(' ', '', ReplaceAll);

        /* generate the link */
        "<br><a href=\"http://ifdb.tads.org/viewgame?ifid=<<v.htmlify()>>\"
        ><b><i><<getDef('name', 'Untitled').htmlify()
          >></i> on IFDB</b></a><br>\n";
    }


    "<br><span class=notes>\n";
    if ((v = getDef('licensetype', nil)) != nil)
        "License: <<v.htmlify()>><br>\n";
    if ((v = getDef('ifid', nil)) != nil)
        "IFID: <<v.htmlify()>><br>\n";

    "</span></span></table>\n";

    /* add the main HTML or plain-text description, as found */
    if ((v = getDef('htmldesc', nil)) != nil)
        "<p><<v>>\n";
    else if ((v = getDef('desc', nil)) != nil)
    {
        /* htmlify it */
        v = v.htmlify();

        /* scan for special characters in the description */
        for (local ofs = 0 ; (ofs = v.find('\\', ofs + 1)) != nil ; )
        {
            switch (v.substr(ofs + 1, 1))
            {
            case '\\':
                v = v.substr(1, ofs) + v.substr(ofs + 2);
                break;

            case 'n':
                v = v.substr(1, ofs - 1) + '<p>' + v.substr(ofs + 2);
                break;

            default:
                break;
            }
        }

        /* write it out */
        "<p><<v>>\n";
    }

    /* start the download block */
    "<td>&nbsp;&nbsp;&nbsp;\n
    <td width=280 align=right>\n
    <table class=downloads width=\"100%\">\n
    <tr><td>\n

    <p><h3>Download</h3>\n";

    /* add the version number */
    if ((v = getDef('version', nil)) != nil)
        "<span class=notes>Version <<v.htmlify()>></span>\n";

    "<table>\n";

    /* add the Windows SETUP package, if present */
    addDownload('setupfile', 'winicon.gif', 'Story Installer (Windows)',
                'For Windows systems, 9x/NT and later. Just download
                and run this SETUP program to install the game.',
                'application');

    /* add the release ZIP file, if present */
    addDownload('zipfile', 'porticon.gif', 'Story Files (All Systems)',
                'Universal edition, for all systems; ZIP format. To play,
                you\'ll need an <a href="http://www.info-zip.org">UnZip
                tool</a> to unpack the files, plus a TADS Interpreter for
                your system.  Visit <a href="http://www.tads.org/tads'
                + getDef('tadsver', '3') + '.htm">tads.org</a> for TADS
                Interpreter downloads.',
                'zip');

    /* add the release game file, if present */
    addDownload('gamefile', 'porticon.gif', 'Story File (All Systems)',
                'Universal edition, for all systems. To play, you\'ll
                need a TADS Interpreter for your system.  Visit
                <a href="http://www.tads.org/tads'
                + getDef('tadsver', '3') + '.htm">tads.org</a> for
                TADS Interpreter downloads.',
                'TADS story file');

    /* add the source file if present */
    addDownload('sourcefile', 'srcicon.gif', 'Source Files',
                'The TADS source files for the game, as a downloadable
                ZIP archive.', 'zip');

    /* add each Web Page Extra file */
    if ((v = global.valTab['extra.file']) != nil)
    {
        /* get the value vector */
        v = v.vals;

        /* get the title vector, if present */
        local titles = global.valTab['extra.title'];
        if (titles != nil)
            titles = titles.vals;

        /* iterate through the extra files */
        for (local i = 1 ; i <= v.length() ; ++i)
        {
            local title;

            /*
             *   Get the title.  If there's an extra.title entry, use
             *   it; otherwise, use the root filename, converted to
             *   title case and cleaned up a bit.
             */
            if (titles != nil && i <= titles.length() && titles[i] != '')
            {
                /* we have an explicit title - use it */
                title = titles[i];
            }
            else
            {
                /* no explicit title - start with the root filename */
                title = winGetRootName(v[i]);

                /* remove any extension */
                if (rexMatch('(.+)<dot><^dot>*$', title) != nil)
                    title = rexGroup(1)[3];

                /* convert underscores to spaces */
                title = title.findReplace('_', ' ', ReplaceAll);

                /* capitalize each letter after a space */
                for (local idx = 0 ; idx != nil && idx < title.length() ;
                     idx = rexSearch('<space><lower>', title, idx + 1))
                {
                    title = title.substr(1, idx)
                        + title.substr(idx + 1, 1).toUpper()
                        + title.substr(idx + 2);
                }

                /* insert a space before each embedded capital */
                for (local idx = rexSearch('<^upper><upper>', title) ;
                     idx != nil ;
                     idx = rexSearch('<^upper><upper>', title, idx[1] + 2))
                {
                    title = title.substr(1, idx[1])
                        + ' '
                        + title.substr(idx[1] + 1);
                }
            }

            /* add the link */
            addDownloadFile(v[i], nil, title, nil,
                            formatFileType(v[i], 'download').toLower());
        }
    }

    /* close the tables and finish up */
    "</table>\n

    <tr height=*>
    </table>
    </table>

    <br><br><br><br>
    <span class=notes>
    <i><<getDef('name', 'Untitled').htmlify()>></i> was created with
    <a href=\"http://www.tads.org\">TADS</a> version
    <<getDef('tadsver', '3')>>.\n

    </body>\n
    </html>\n";
}

/*
 *   Add a download link to the page, given a variable name containing
 *   a filename.
 */
addDownload(varname, iconfile, title, desc, ftype)
{
    /* look up the variable - if not defined, skip this download link */
    local fname = getDef(varname, nil);
    if (fname == nil)
        return;

    /* add this file */
    addDownloadFile(fname, iconfile, title, desc, ftype);
}

/*
 *   add a download link, given the filename
 */
addDownloadFile(fname, iconfile, title, desc, ftype)
{
    /* copy the project file */
    copyProjectFile(fname);

    /* copy the icon file */
    if (iconfile != nil)
        copySysFile(iconfile);

    /* add the icon */
    "<tr valign=top>\n
    <td>\n";
    if (iconfile != nil)
        "<a href=\"<<winGetRootName(fname)>>\"><img src=\"<<iconfile>>\"
        border=0></a>\n";

    /* add the title link */
    "<td class=dltext>\n
    <a href=\"<<winGetRootName(fname)>>\"><b><<title>></b></a>\n";

    /* add the description, if present */
    if (desc != nil)
        "<br>\n
        <span class=notes>\n
        <<desc>>\n";
    else
        "<span class=notes>\n";

    /* add the file format and size */
    "<br><span class=fileinfo>\n
    (<<ftype>>, <<formatFileSize(fname)>>)\n
    </span>\n

    </span>\n";
}


/* ------------------------------------------------------------------------ */
/*
 *   Main command-line entrypoint.
 *
 *   The command line looks like this:
 *
 *   name=value name=value ...
 *
 *   where 'name' is a symbol name, and 'val' is a value string to use
 *   for the name.  For a list of the names that Workbench supplies,
 *   see the comment at the start of the file.
 */
main(args)
{
    /* remember our program directory */
    global.progDir = getDirPath(args[1]);

    /* parse the arguments and GameInfo.txt */
    global.valTab = parseArgs(args);

    /* for debugging only - display the name/value table */
    //displayValTab();

    /* make sure we have an output directory */
    global.outDir = getRequired('out', 'output directory');

    /* create the output file */
    try
    {
        global.outFile = File.openTextFile(
            makePath(global.outDir, 'index.htm'),
            FileAccessWrite, 'ISO-8859-1');
    }
    catch (Exception exc)
    {
        "Error creating index.htm: ";
        throw(exc);
    }

    /* set the default output function so that we write this file */
    t3SetSay(indexSay);

    try
    {
        /* generate the page */
        generatePage();
    }
    finally
    {
        /*
         *   restore the default output function before terminating -
         *   this ensures that any uncaught exceptions will be
         *   displayed to the console, not written to the index file
         */
        t3SetSay(_default_display_fn);
    }

    /* done - close the output file */
    global.outFile.closeFile();
}

/* ------------------------------------------------------------------------ */
/*
 *   Copy a file.  This simply makes a byte-for-byte copy of the source
 *   file, saving it under the destination name.
 */
copyFile(srcName, dstName)
{
    /* convert any forward slashes in the names to backslashes */
    srcName = srcName.findReplace('/', '\\', ReplaceAll);
    dstName = dstName.findReplace('/', '\\', ReplaceAll);

    /* open the input and output files */
    local fSrc;
    try
    {
        fSrc = File.openRawFile(srcName, FileAccessRead);
    }
    catch (Exception exc)
    {
        "Error opening <<srcName>>: ";
        throw exc;
    }

    local fDst;
    try
    {
        fDst = File.openRawFile(dstName, FileAccessWrite);
    }
    catch (Exception exc)
    {
        "Error creating <<dstName>>: ";
        throw exc;
    }

    /* copy the data */
    for (local b = new ByteArray(16384) ; ; )
    {
        /* read the next chunk */
        local len = fSrc.readBytes(b);
        if (len == 0)
            break;

        /* write it out */
        fDst.writeBytes(b, 1, len);
    }

    /* close the files */
    fSrc.closeFile();
    fDst.closeFile();
}

/*
 *   Copy a project file - this copies a given file from the project to
 *   the output directory.  The output file has the same root name as
 *   the source file, but it stored in the output directory.
 */
copyProjectFile(fname)
{
    copyFile(fname, makePath(global.outDir, winGetRootName(fname)));
}

/*
 *   Copy a "system" file - this copies a file from our program
 *   directory to the output directory
 */
copySysFile(fname)
{
    copyFile(makePath(global.progDir, fname),
             makePath(global.outDir, fname));
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the size of a file, as a displayable string.  Returns the size
 *   in suitable units - GB, MB, KB, or bytes.
 */
formatFileSize(fname)
{
    /* get the size of the file in bytes */
    local f;
    try
    {
        f = File.openRawFile(fname, FileAccessRead);
    }
    catch (Exception exc)
    {
        "Error opening <<fname>>: ";
        throw exc;
    }
    local sz = f.getFileSize();
    f.closeFile();

    /* format it according to the size */
    if (sz >= 1024*1024*1024)
        return ((new BigNumber(sz) / 1073741824.0)
                .formatString(5, 0, 1, 2)) + 'GB';
    if (sz >= 10*1024*1024)
        return sz/(1024*1024) + 'MB';
    if (sz >= 1024*1024)
        return ((new BigNumber(sz) / 1048576.0)
                .formatString(5, 0, 1, 2)) + 'MB';
    if (sz >= 10*1024)
        return toString(sz/1024) + 'KB';
    return toString(sz) + ' bytes';
}

/* ------------------------------------------------------------------------ */
/*
 *   Format a date in the GameInfo format (YYYY-MM-DD) in fully spelled-out
 *   English format ("January 1, 1980")
 */
formatDate(dval)
{
    /* parse the yyyy-mm-dd format */
    if (rexMatch('(<digit>{4})-(<digit>{2})-(<digit>{2})$', dval) == nil)
        return dval;

    /* pull out the parts */
    local yyyy = toInteger(rexGroup(1)[3]),
        mm = toInteger(rexGroup(2)[3]),
        dd = toInteger(rexGroup(3)[3]);

    /* validate the month */
    if (mm < 1 || mm > 12)
        return dval;

    /* format it */
    local months = ['January', 'February', 'March', 'April', 'May', 'June',
        'July', 'August', 'September', 'October', 'November', 'December'];
    return months[mm] + ' ' + toString(dd) + ', ' + toString(yyyy);

}

/* ------------------------------------------------------------------------ */
/*
 *   Get the type of a file as a displayable format name.  We look for
 *   common extensions, and return corresponding format names; if we
 *   can't find any of the common extensions we know, we simply return
 *   the extension string.
 *
 *   If the file doesn't have an extension, or it has an extension
 *   commonly associated with arbitrary binary data files (such as
 *   ".dat" or ".bin"), we'll return the given default name.
 */
formatFileType(fname, defName)
{
    local extmap =
    [
        'dat', nil,
        'bin', nil,
        'txt', 'Text',
        'htm', 'HTML',
        'html', 'HTML',
        'xml', 'XML',
        'pdf', 'PDF',
        'zip', 'Zip',
        'jpg', 'JPEG Image',
        'jpe', 'JPEG Image',
        'jpeg', 'JPEG Image',
        'png', 'PNG Image',
        't3', 'TADS Story',
        'gam', 'TADS Story',
        'sav', 'Saved Game',
        't3v', 'Saved Game',
        'exe', 'Application',
        'doc', 'Document'
    ];

    /* pull out the extension, if any */
    if (rexSearch('<dot>(<^dot>+)$', fname) == nil)
        return defName;
    local ext = rexGroup(1)[3];

    /* search for the extension in our list */
    local i;
    for (i = 1 ; i <= extmap.length() ; i += 2)
    {
        /* check for a match */
        if (extmap[i] == ext)
        {
            /*
             *   it's a match - return this extension's format name, or
             *   the default name if the format for the extension is
             *   unknown
             */
            return extmap[i+1] != nil ? extmap[i+1] : defName;
        }
    }

    /* didn't find it - just return the extension itself in title case */
    return ext.substr(1, 1).toUpper() + ext.substr(2).toLower();
}

/* ------------------------------------------------------------------------ */
/*
 *   Miscellaneous global variables
 */
global: object
    /* main index.htm output file (File object) */
    outFile = nil

    /*
     *   the output directory - this is simply the 'out' parameter, but
     *   we store it here for easy access, since it's needed frequently
     */
    outDir = nil

    /*
     *   The program directory.  We set this up in main() to the
     *   directory path from args[1], which is the name of the running
     *   t3 image file; this is the directory where we'd normally look
     *   for any standard resources we reference, such as our style
     *   sheet and icon images.
     */
    progDir = nil

    /* the name/value table */
    valTab = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Index file output function.  To simplify writing the main page
 *   generator, we set this up as the default output function; so, any
 *   double-quoted strings will be interpreted as writing to the main
 *   index file.
 */
indexSay(val)
{
    global.outFile.writeFile(val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a full Windows/DOS path name, given the directory and the
 *   name of the file.  This routine is Windows-specific.
 */
makePath(dir, fname)
{
    /*
     *   if there's no directory path, the full path is simply the
     *   filename
     */
    if (dir == '')
        return fname;

    /* convert any forward slashes in the path to backslashes */
    dir = dir.findReplace('/', '\\', ReplaceAll);

    /* if the directory doesn't end in a backslash, add one */
    if (!dir.endsWith('\\'))
        dir += '\\';

    /* the result is the directory path plus the filename */
    return dir + fname;
}

/*
 *   Get the root name of a filename.  This is the filename with any
 *   directory path portion removed.  This routine is Windows-specific.
 */
winGetRootName(fname)
{
    /* parse the filename as 'path\name' or 'path:name' */
    if (rexMatch('(.*)[:\\]([^\\:]*)$', fname) == nil)
    {
        /* no path separators - the root name is the whole thing */
        return fname;
    }
    else
    {
        /* found a path separator - the root name is the last part */
        return rexGroup(2)[3];
    }
}

/*
 *   Get the directory path from a filename.  This routine is
 *   Windows-specific.
 */
getDirPath(fname)
{
    /* parse the filename as 'path\name' or 'path:name' */
    if (rexMatch('(.*)[:\\]([^\\:])*$', fname) == nil)
    {
        /* no path separators - there's no path */
        return '';
    }
    else
    {
        /* the path is the part before the last separator */
        return rexGroup(1)[3];
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up a value, returning the given default if the value isn't
 *   defined.
 */
getDef(name, defVal)
{
    local v;

    /*
     *   if it's defined, return its value; otherwise return the given
     *   default
     */
    if ((v = global.valTab[name]) != nil)
        return v.getVal();
    else
        return defVal;
}

/*
 *   Look up a required value.  Returns the value if present, or throws
 *   an error if not.
 */
getRequired(name, desc)
{
    local v;

    /* look up the name; if it's not defined, it's an error */
    if ((v = global.valTab[name]) == nil)
        throw new ProgramException('"' + name + '" ('
                                   + desc + ') not defined');

    /* got it - return the value */
    return v.getVal();
}


/* ------------------------------------------------------------------------ */
/*
 *   Name/Value entry.
 */
class ArgValue: object
    /*
     *   The values.  Each variable name can appear more than once, so
     *   we need a vector of values.
     */
    vals = perInstance(new Vector(5))

    /* add a value to my list */
    addVal(val) { vals.append(val); }

    /*
     *   get the first value - most name/value pairs are single-valued,
     *   so this simplifies retrieval in the common case
     */
    getVal() { return vals[1]; }

    /* get the nth value from a multi-valued variable */
    idxVal(n) { return vals[n]; }
;

/* ------------------------------------------------------------------------ */
/*
 *   Parse the command line, loading the name/value pairs.  We'll then find
 *   the GameInfo.txt file reference in the arguments, and we'll parse the
 *   contents of that file as well.
 *
 *   Creates global.valTab as a lookup table, filled in with all of the
 *   name/value pairs we read from the command-line arguments and from the
 *   GameInfo file. The table is keyed by name, and the value associated
 *   with each name is an ArgValue object giving the value or values
 *   defined for the name.
 *
 *   'args' is the command-line argument list, as passed into main().
 */
parseArgs(args)
{
    local tab = global.valTab = new LookupTable(64, 128);

    /* run through the arguments, and remember each name/value pair */
    for (local i = 2, local len = args.length() ; i <= len ; ++i)
    {
        /* parse this argument - each is in 'name=value' format */
        if (rexMatch('(<alphanum|dot>+)=(.*)$', args[i]) == nil)
            throw new ProgramException(
                'invalid argument format: ' + args[i]);

        /* pull out the variable name and value */
        local name = rexGroup(1)[3].toLower();
        local val = rexGroup(2)[3];

        /* if this name isn't already in the table, add it */
        local entry = tab[name];
        if (entry == nil)
            tab[name] = entry = new ArgValue();

        /* add the value to the entry */
        entry.addVal(val);
    }

    /* open the GameInfo file */
    local fname = getRequired('info', 'GameInfo filename');
    local gif;
    try
    {
        gif = File.openTextFile(fname, FileAccessRead, 'utf-8');
    }
    catch (Exception exc)
    {
        "Error opening <<fname>>: ";
        throw exc;
    }

    /* read the file */
    for (local s = gif.readFile() ; s != nil ; )
    {
        /* remove the trailing newline, if present */
        if (s.endsWith('\n'))
            s = s.substr(1, s.length() - 1);

        /* trim leading spaces */
        local len = rexMatch('<space>+', s);
        if (len != nil)
            s = s.substr(len + 1);

        /* skip comment and blank lines */
        if (s == '' || s.startsWith('#'))
        {
            s = gif.readFile();
            continue;
        }

        /* parse the 'name: value' format; if that fails, skip the line */
        if (rexMatch('(<alphanum>+)<space>*:<space>*(.*)$', s) == nil)
        {
            s = gif.readFile();
            continue;
        }

        /* pull out the name and value */
        local name = rexGroup(1)[3].toLower();
        local val = rexGroup(2)[3];

        /* read any continuation lines */
        for (;;)
        {
            /* read the next line */
            if ((s = gif.readFile()) == nil)
                break;

            /* trim the newline */
            if (s.endsWith('\n'))
                s = s.substr(1, s.length() - 1);

            /*
             *   if it doesn't start with a space, it's not a
             *   continuation line
             */
            if ((len = rexMatch('<space>+', s)) == nil)
                break;

            /* trim off leading spaces */
            s = s.substr(len + 1);

            /* add a space for the newline, plus the continuation text */
            val += ' ' + s;
        }

        /* add this value to the name/value pair table */
        local entry = tab[name];
        if (entry == nil)
            tab[name] = entry = new ArgValue();
        entry.addVal(val);
    }

    /* done with the file */
    gif.closeFile();

    /* return the table */
    return tab;
}

/* ------------------------------------------------------------------------ */
/*
 *   Display the name/value table (for debugging purposes)
 */
displayValTab()
{
    global.valTab.forEachAssoc(new function(key, val)
    {
        if (val.vals.length() == 1)
            tadsSay(key + ' = "' + val.getVal().htmlify() + '"\n');
        else
        {
            for (local i = 1, local len = val.vals.length() ; i <= len ; ++i)
                tadsSay(key + '[' + i + '] = "' + val.idxVal(i).htmlify() + '"\n');
        }
    });
}
