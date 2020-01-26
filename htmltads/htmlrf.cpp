#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlrf.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  htmlrf.cpp - HTML resource finder
Function

Notes

Modified
  12/03/97 MJRoberts  - Creation
*/

/* include TADS os interface functions */
#include <os.h>

#ifndef HTMLRF_H
#include "htmlrf.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry subclass for a resource map entry
 */
class CHtmlHashEntryFileres: public CHtmlHashEntryCI
{
public:
    CHtmlHashEntryFileres(const textchar_t *str, size_t len,
                          unsigned long seekofs, unsigned long siz,
                          int fileno)
        : CHtmlHashEntryCI(str, len, TRUE)
    {
        seekofs_ = seekofs;
        siz_ = siz;
        fileno_ = fileno;
    }

    CHtmlHashEntryFileres(const textchar_t *str, size_t len,
                          const textchar_t *linkstr, size_t linklen)
        : CHtmlHashEntryCI(str, len, TRUE)
    {
        seekofs_ = 0;
        siz_ = 0;
        fileno_ = 0;
        link_.set(linkstr, linklen);
    }

    /* seek offset and size of the resource in the resource file */
    unsigned long seekofs_;
    unsigned long siz_;

    /*
     *   File number - the resource finder keeps a list of resource files
     *   that we've mapped; this gives the ID of the file in the list.
     */
    int fileno_;

    /*
     *   Linked filename.  Resources can be given as links to the local file
     *   system rather than as stored binary data.  This is mostly used for
     *   debugging, to save the time and disk space of making a copy of the
     *   binary resource data when the local files are all present anyway.
     */
    CStringBuf link_;
};

/* ------------------------------------------------------------------------ */
/*
 *   File list entry.  The resource finder keeps a list of files that
 *   we've loaded.
 */
class CHtmlRfFile
{
public:
    CHtmlRfFile()
    {
        /* assume the seek base is zero */
        res_seek_base_ = 0;
    }

    CHtmlRfFile(const textchar_t *fname)
    {
        /* set the filename */
        filename_.set(fname);

        /* we don't know the seek base yet - assume it will be zero */
        res_seek_base_ = 0;
    }

    /* get the filename */
    const textchar_t *get_fname() { return filename_.get(); }

    /* change the filename */
    void set_fname(const textchar_t *fname) { filename_.set(fname); }

    /* get/set the seek base position */
    unsigned long get_seek_base() const { return res_seek_base_; }
    void set_seek_base(unsigned long val) { res_seek_base_ = val; }

private:
    /* name of the file */
    CStringBuf filename_;

    /* base seek offset of resources in .GAM file */
    unsigned long res_seek_base_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Resource finder implementation
 */

CHtmlResFinder::CHtmlResFinder()
{
    /* initialize */
    init_data();
}

CHtmlResFinder::~CHtmlResFinder()
{
    /* delete the old data */
    delete_data();
}

/*
 *   initialize our data
 */
void CHtmlResFinder::init_data()
{
    /* create a hash table for the resource map */
    restab_ = new CHtmlHashTable(256, new CHtmlHashFuncCI);

    /*
     *   Initialize the file list.  We always allocate the first entry,
     *   since this is the dedicated entry for the .GAM file.
     */
    files_alloced_ = 5;
    files_ = (CHtmlRfFile **)
             th_malloc(files_alloced_ * sizeof(CHtmlRfFile *));
    files_used_ = 1;
    files_[0] = new CHtmlRfFile();

    /* we have no external resource file search path directories yet */
    search_path_head_ = search_path_tail_ = 0;

    /* assume we're running as a normal interpreter (not a debugger) */
    debugger_mode_ = FALSE;
}

/*
 *   Delete our data
 */
void CHtmlResFinder::delete_data()
{
    int i;

    /* delete the hash table */
    delete restab_;

    /* delete the list of files */
    for (i = 0 ; i < files_used_ ; ++i)
        delete files_[i];
    th_free(files_);

    /* delete the external file search path */
    while (search_path_head_ != 0)
    {
        /* remember the next one */
        CHtmlResFinderPathEntry *nxt = search_path_head_->get_next();

        /* delete this entry */
        delete search_path_head_;

        /* unlink it */
        search_path_head_ = nxt;
    }
}

/*
 *   Reset
 */
void CHtmlResFinder::reset()
{
    /* delete the old data */
    delete_data();

    /* re-initialize */
    init_data();
}

/*
 *   Determine if an entry exists
 */
int CHtmlResFinder::resfile_exists(const textchar_t *resname,
                                   size_t resnamelen)
{
    char cvtbuf[OSFNMAX];

    /*
     *   see if we can find an entry in the resource map - if so, it
     *   definitely exists
     */
    CHtmlHashEntryFileres *entry =
        (CHtmlHashEntryFileres *)restab_->find(resname, resnamelen);
    if (entry != 0)
        return TRUE;

    /* check with the local system to see if it's an embedded resource */
    if (CHtmlSysFrame::get_frame_obj() != 0)
    {
        /* ask the system for the resource */
        unsigned long pos;
        unsigned long siz;
        if (CHtmlSysFrame::get_frame_obj()->get_exe_resource(
            resname, resnamelen, cvtbuf, sizeof(cvtbuf), &pos, &siz))
            return TRUE;
    }

    /*
     *   There's no entry in the resource map matching this name - map the
     *   resource name to a stand-alone external filename.  If that fails,
     *   return failure.
     */
    if (!resname_to_filename(
        cvtbuf, sizeof(cvtbuf), resname, resnamelen, TRUE))
        return FALSE;

    /*
     *   if the file exists, the resource exists - so, if we don't get an
     *   error code back from osfacc, return true
     */
    return !osfacc(cvtbuf);
}

/*
 *   Get the filename and seek offset to use to read a resource.  If we
 *   can find the resource in our .GAM file resource map, we'll return the
 *   GAM filename and the offset of the resource in the .GAM file;
 *   otherwise, we'll return the name of the resource itself and a seek
 *   offset of zero.
 */
void CHtmlResFinder::get_file_info(CStringBuf *outbuf,
                                   const textchar_t *resname,
                                   size_t resnamelen,
                                   unsigned long *start_seekpos,
                                   unsigned long *file_size)
{
    /* find the resource */
    if (!find_resource(outbuf, resname, resnamelen,
                       start_seekpos, file_size, 0))
    {
        /* failed - log a debug message */
        oshtml_dbg_printf("resource loader error: resource not found: %s\n",
                          resname);
    }
}

/*
 *   Find a resource.  Returns true if we find the resource, false if not.
 *
 *   If we find the resource, and fp_ret is not null, we'll fill in *fp_ret
 *   with an osfildef* handle to the file, with its seek position set to the
 *   start of the resource.
 *
 *   If *filenamebuf is non-null, we'll fill it in with the name of the file
 *   containing the resource.
 */
int CHtmlResFinder::find_resource(CStringBuf *filenamebuf,
                                  const char *resname,
                                  size_t resnamelen,
                                  unsigned long *start_seekpos,
                                  unsigned long *res_size,
                                  osfildef **fp_ret)
{
    char cvtbuf[OSFNMAX];
    const char *fname;
    size_t fnamelen;
    int is_url;

    /* see if we can find an entry in the resource map */
    CHtmlHashEntryFileres *entry =
        (CHtmlHashEntryFileres *)restab_->find(resname, resnamelen);
    if (entry != 0 && entry->link_.get() == 0)
    {
        /*
         *   Found an entry - return the seek position of the entry in the
         *   file (calculate the actual seek position as the resource map
         *   base seek address plus the offset of this resource within the
         *   resource map).  Get the filename from the files_[] list entry
         *   for the file number stored in the resource entry.
         */
        *start_seekpos = files_[entry->fileno_]->get_seek_base()
                         + entry->seekofs_;
        *res_size = entry->siz_;

        /* tell the caller the name, if desired */
        if (filenamebuf != 0)
            filenamebuf->set(files_[entry->fileno_]->get_fname());

        /* if the caller wants the file handle, open the file */
        if (fp_ret != 0)
        {
            /* open the file */
            *fp_ret = osfoprb(files_[entry->fileno_]->get_fname(), OSFTBIN);

            /* seek to the start of the file */
            if (*fp_ret != 0)
                osfseek(*fp_ret, *start_seekpos, OSFSK_SET);
        }

        /* indicate that we found the resource */
        return TRUE;
    }
    else if (entry != 0)
    {
        /* it's a linked resource - get the local filename from the link */
        fname = entry->link_.get();
        fnamelen = strlen(fname);
        is_url = FALSE;
    }
    else
    {
        /*
         *   check with the local system to see if it's a native embedded
         *   resource (that is, embedded in the application executable, such
         *   as a Mac resource fork item or a Windows .EXE resource)
         */
        if (CHtmlSysFrame::get_frame_obj() != 0)
        {
            /* ask the system for the resource */
            if (CHtmlSysFrame::get_frame_obj()->get_exe_resource(
                resname, resnamelen, cvtbuf, sizeof(cvtbuf),
                start_seekpos, res_size))
            {
                /* it's an embedded resource - tell them the filename */
                if (filenamebuf != 0)
                    filenamebuf->set(cvtbuf);

                /* open the file if the caller is interested */
                if (fp_ret != 0)
                {
                    /* open the file */
                    *fp_ret = osfoprb(cvtbuf, OSFTBIN);

                    /* seek to the start of the resource */
                    if (*fp_ret != 0)
                        osfseek(*fp_ret, *start_seekpos, OSFSK_SET);
                }

                /* indicate that we found the resource */
                return TRUE;
            }
        }

        /* use the resource name as a filename, after URL conversion */
        fname = resname;
        fnamelen = resnamelen;
        is_url = TRUE;
    }

    /*
     *   There's no entry in the resource map matching this name.  Map the
     *   resource name to a local filename in the resource directory; if that
     *   fails, there's nowhere else to look, of return failure.
     */
    if (!resname_to_filename(cvtbuf, sizeof(cvtbuf), fname, fnamelen, is_url))
    {
        if (fp_ret != 0)
            *fp_ret = 0;
        *res_size = 0;
        return FALSE;
    }

    /*
     *   when resource comes from an external local file, the entire file is
     *   the single resource, so the resource data starts at offset zero
     */
    *start_seekpos = 0;

    /* store the name of the stand-alone external file */
    if (filenamebuf != 0)
        filenamebuf->set(cvtbuf);

    /* try opening the file */
    osfildef *fp = osfoprb(cvtbuf, OSFTBIN);

    /*
     *   if we failed, and the path looks like an "absolute" local path, try
     *   opening the resource name directly, without any url-to-local
     *   translations
     */
    if (fp == 0)
    {
        /* make a null-terminated copy of the original resource name */
        if (fnamelen > sizeof(cvtbuf) - 1)
            fnamelen = sizeof(cvtbuf) - 1;
        memcpy(cvtbuf, fname, fnamelen);
        cvtbuf[fnamelen] = '\0';

        /* if the path looks absolute, try opening it as a local file */
        if (os_is_file_absolute(cvtbuf))
        {
            /* store the name of the external file */
            if (filenamebuf != 0)
                filenamebuf->set(cvtbuf);

            /* try opening the file */
            fp = osfoprb(cvtbuf, OSFTBIN);
        }
    }

    /* check to see if we got a file */
    if (fp == 0)
    {
        /* no file - indicate failure */
        *res_size = 0;
        return FALSE;
    }

    /* seek to the end of the file to determine its size */
    osfseek(fp, 0, OSFSK_END);
    *res_size = (unsigned long)osfpos(fp);

    /* if the caller wants the file, return it; otherwise, close it */
    if (fp_ret != 0)
    {
        /*
         *   position the file back at the start of the resource data (which
         *   is simply the start of the file, since the entire file is the
         *   resource's data)
         */
        osfseek(fp, 0, OSFSK_SET);

        /* give the handle to the caller */
        *fp_ret = fp;
    }
    else
    {
        /* they don't want it - close the file */
        osfcls(fp);
    }

    /* indicate success */
    return TRUE;
}

/*
 *   Given a resource name, generate the external filename for the
 *   resource
 */
int CHtmlResFinder::resname_to_filename(
    char *outbuf, size_t outbufsize,
    const char *resname, size_t resname_len, int is_url)
{
    char buf[OSFNMAX];
    char rel_fname[OSFNMAX];

    /* make sure the file safety level allows read access to external files */
    int read_safety = 4, write_safety = 4;
    if (appctx_ != 0 && appctx_->get_io_safety_level != 0)
        appctx_->get_io_safety_level(appctx_->io_safety_level_ctx,
                                     &read_safety, &write_safety);
    if (read_safety > 3)
        return FALSE;

    /* generate a null-terminated version of the resource name */
    if (resname_len > sizeof(buf) - 1)
        resname_len = sizeof(buf) - 1;
    memcpy(buf, resname, resname_len);
    buf[resname_len] = '\0';

    /* convert the URL to a filename, if desired */
    if (is_url)
        os_cvt_url_dir(rel_fname, sizeof(rel_fname), buf);
    else
        strcpy(rel_fname, buf);

    /*
     *   If we have a non-URL absolute path, and we're running as a debugger,
     *   allow access without enforcing any sandbox restrictions.  In debug
     *   builds, the compiler embed links to resource files rather than
     *   actually embedding the resource data.  These links can come from
     *   library folders or elsewhere.  In normal interpreter mode, this
     *   could pose a security risk, as a malicious game could embed a link
     *   to an absolute path to a system file - so we have to sandbox links
     *   in interpreter mode, just as we sandbox any direct file reference.
     *   But in debugger mode, the user is presumably working with the game's
     *   source code, and in most cases with his or her own source code, so
     *   we can assume that the code is more trusted.
     */
    if (debugger_mode_
        && !is_url
        && os_is_file_absolute(rel_fname)
        && !osfacc(rel_fname))
    {
        safe_strcpy(outbuf, outbufsize, rel_fname);
        return TRUE;
    }

    /*
     *   Get the resource path prefix.  If we have an explicit resource root
     *   directory, use that.  Otherwise, use the path portion of the game
     *   filename, so that we look for individual resources in the same
     *   directory containing the game file.
     */
    if (res_dir_.get() != 0 && res_dir_.get()[0] != '\0')
    {
        /* use the resource directory */
        os_build_full_path(outbuf, outbufsize, res_dir_.get(), rel_fname);

        /*
         *   Make sure the result is within this directory; if so, and it
         *   exists, return this file.  The same-directory check is for file
         *   safety purposes: we don't want to let the game peek outside the
         *   sandbox using ".." paths, symlinks, etc.  If the file safety
         *   level is 1 or less (1 is "read any/write local", 0 is "read
         *   any/write any"), allow read access regardless of location.
         */
        if ((read_safety <= 1
             || os_is_file_in_dir(outbuf, res_dir_.get(), TRUE, FALSE))
            && !osfacc(outbuf))
            return TRUE;
    }
    else if (files_[0]->get_fname() != 0)
    {
        /* get the path name from the game filename */
        os_get_path_name(buf, sizeof(buf), files_[0]->get_fname());

        /* build the full path */
        os_build_full_path(outbuf, outbufsize, buf, rel_fname);

        /* if the result exists and satifies file safety, return it */
        if ((read_safety <= 1
             || os_is_file_in_dir(outbuf, buf, TRUE, FALSE))
            && !osfacc(outbuf))
            return TRUE;
    }
    else
    {
        /*
         *   we have no root path; just use the relative filename as it is;
         *   limit the copy length to the buffer size
         */
        size_t len = strlen(rel_fname);
        if (len > outbufsize)
            len = outbufsize - 1;

        /* copy and null-terminate the result */
        memcpy(outbuf, rel_fname, len);
        outbuf[len] = '\0';

        /* if the result exists and satifies file safety, return it */
        if ((read_safety <= 1
             || os_is_file_in_dir(outbuf, OSPATHPWD, TRUE, FALSE))
            && !osfacc(outbuf))
            return TRUE;
    }

    /* no luck with the basic resource directory; check the search path */
    for (const CHtmlResFinderPathEntry *entry = search_path_head_ ;
         entry != 0 ; entry = entry->get_next())
    {
        /* try building this path into a full filename */
        os_build_full_path(outbuf, outbufsize, entry->get_path(), rel_fname);

        /* if the result exists and satifies file safety, return it */
        if ((read_safety <= 1
             || os_is_file_in_dir(outbuf, entry->get_path(), TRUE, FALSE))
            && !osfacc(outbuf))
            return TRUE;
    }

    /* no luck finding the file */
    return FALSE;
}

/*
 *   Set the name of the game file
 */
void CHtmlResFinder::set_game_name(const char *fname)
{
    /*
     *   Remember the name of the game file - we'll use this to look up
     *   resources.  The first entry in our file list is dedicated to the
     *   GAM file, so set this entry's filename.  In addition, we'll use
     *   the path to the game when looking for resources, since we look in
     *   the game's directory for external resources.
     */
    files_[0]->set_fname(fname);
}

/*
 *   Set the root directory path for individual resources
 */
void CHtmlResFinder::set_res_dir(const char *dir)
{
    /* remember the name */
    res_dir_.set(dir);
}

/*
 *   Add a resource.  We'll add an entry to our hash table with the
 *   resource information.
 */
void CHtmlResFinder::add_resource(unsigned long ofs, unsigned long siz,
                                  const char *nm, size_t nmlen, int fileno)
{
    /* create and add a new hash table entry */
    restab_->add(new CHtmlHashEntryFileres(nm, nmlen, ofs, siz, fileno));
}

/*
 *   Add a resource link.  This is used mainly for debugging purposes, to
 *   create links from resource names directly to local files, rather than
 *   copying the resource binary data into the game file.
 */
void CHtmlResFinder::add_resource(const char *fname, size_t fnamelen,
                                  const char *resnm, size_t resnmlen)
{
    /* create and add a new hash entry */
    restab_->add(new CHtmlHashEntryFileres(resnm, resnmlen, fname, fnamelen));
}

/*
 *   Add a resource file search path.
 */
void CHtmlResFinder::add_res_path(const char *path, size_t len)
{
    /* create a search path entry */
    CHtmlResFinderPathEntry *entry = new CHtmlResFinderPathEntry(path, len);

    /* link it in at the end of our list */
    if (search_path_tail_ != 0)
        search_path_tail_->set_next(entry);
    else
        search_path_head_ = entry;
    search_path_tail_ = entry;
}

/*
 *   Set the resource seek base for a resource file
 */
void CHtmlResFinder::set_resmap_seek(unsigned long seekpos, int fileno)
{
    files_[fileno]->set_seek_base(seekpos);
}

/*
 *   Add a resource file, returning the file number for the new resource
 *   file.
 */
int CHtmlResFinder::add_resfile(const char *fname)
{
    int fileno;

    /* note the new file's index */
    fileno = files_used_;

    /*
     *   If there isn't room in our array of resource files, expand the
     *   array.
     */
    if (files_used_ == files_alloced_)
    {
        files_alloced_ += 5;
        files_ = (CHtmlRfFile **)
                 th_realloc(files_, files_alloced_ * sizeof(CHtmlRfFile *));
    }

    /* add the new entry */
    files_[files_used_] = new CHtmlRfFile(fname);
    ++files_used_;

    /* return the new file number */
    return fileno;
}


/* ------------------------------------------------------------------------ */
/*
 *   Static functions invoked from TADS loader.  We'll refer these to an
 *   instance of a CHtmlResFinder, which is stored in the callback
 *   context.
 */

void CHtmlResFinder::cb_set_game_name(void *ctx, const char *fname)
{
    ((CHtmlResFinder *)ctx)->set_game_name(fname);
}

void CHtmlResFinder::cb_set_res_dir(void *ctx, const char *dir)
{
    ((CHtmlResFinder *)ctx)->set_res_dir(dir);
}

void CHtmlResFinder::cb_set_resmap_seek(void *ctx, unsigned long seekpos,
                                        int fileno)
{
    ((CHtmlResFinder *)ctx)->set_resmap_seek(seekpos, fileno);
}

void CHtmlResFinder::cb_add_resource(void *ctx, unsigned long ofs,
                                     unsigned long siz,
                                     const char *nm, size_t nmlen,
                                     int fileno)
{
    ((CHtmlResFinder *)ctx)->add_resource(ofs, siz, nm, nmlen, fileno);
}

void CHtmlResFinder::cb_add_resource_link(
    void *ctx, const char *fname, size_t fnamelen,
    const char *nm, size_t nmlen)
{
    ((CHtmlResFinder *)ctx)->add_resource(fname, fnamelen, nm, nmlen);
}

int CHtmlResFinder::cb_add_resfile(void *ctx, const char *fname)
{
    return ((CHtmlResFinder *)ctx)->add_resfile(fname);
}

void CHtmlResFinder::cb_add_res_path(void *ctx, const char *path, size_t len)
{
    ((CHtmlResFinder *)ctx)->add_res_path(path, len);
}

/*
 *   Determine if a resource exists
 */
int CHtmlResFinder::cb_resfile_exists(void *ctx, const char *res_name,
                                      size_t res_name_len)
{
    return ((CHtmlResFinder *)ctx)->resfile_exists(res_name, res_name_len);
}

/*
 *   Find a resource
 */
osfildef *CHtmlResFinder::cb_find_resource(void *ctx, const char *resname,
                                           size_t resnamelen,
                                           unsigned long *res_size)
{
    unsigned long start_pos;
    osfildef *fp;

    /* try finding the resource */
    if (((CHtmlResFinder *)ctx)->find_resource(
        0, resname, resnamelen, &start_pos, res_size, &fp))
    {
        /* success - return the file handle */
        return fp;
    }
    else
    {
        /* failed - return a null file handle */
        return 0;
    }
}

