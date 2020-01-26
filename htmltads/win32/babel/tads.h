/*
 *   tads.h - Treaty of Babel common declarations for tads2 and tads3 modules
 *
 *   This file depends on treaty_builder.h
 *
 *   This file is public domain, but note that any changes to this file may
 *   render it noncompliant with the Treaty of Babel
 *
 *   Modified
 *.   04/18/2006 MJRoberts  - creation
 */

#ifndef TADS_H
#define TADS_H

/* match a TADS file signature */
int tads_match_sig(const void *buf, int32 len, const char *sig);

/* get the IFID for a tads story file */
int32 tads_get_story_file_IFID(void *story_file, int32 extent,
                               char *output, int32 output_extent);

/* get the synthesized iFiction record from a tads story file */
int32 tads_get_story_file_metadata(void *story_file, int32 extent,
                                   char *buf, int32 bufsize);

/* get the size of the synthesized iFiction record for a tads story file */
int32 tads_get_story_file_metadata_extent(void *story_file, int32 extent);

/* get the cover art from a tads story file */
int32 tads_get_story_file_cover(void *story_file, int32 extent,
                                void *buf, int32 bufsize);

/* get the size of the cover art from a tads story file */
int32 tads_get_story_file_cover_extent(void *story_file, int32 extent);

/* get the image format (jpeg, png) of the covert art in a tads story file */
int32 tads_get_story_file_cover_format(void *story_file, int32 extent);

/*
 *   Extended interface (for TADS Workbench): turn a GameInfo record into an
 *   iFiction record.
 *
 *   'tads_version' is the major TADS version number: currently this must be
 *   2 or 3.
 *
 *   'gi_txt' and 'gi_len' describe a buffer containing the GameInfo data.
 *
 *   'buf' and 'bufsize' describe the output buffer, which receives the
 *   iFiction record.  If bufsize is zero, we'll simply calculate the size of
 *   the generated iFiction data without actually storing anything; this lets
 *   you determine how big a buffer is needed before allocating it.
 *
 *   The return value is the size of the iFiction data, or a negative xxx_RV
 *   error code if an error occurs.
 *
 *   If bufsize is non-zero, fills in the buffer and returns the number of
 *   bytes stored.  If the output overflows the buffer, returns
 *   INVALID_USAGE_RV.
 *
 *   If bufsize is zero, we'll write nothing to the buffer, but we'll return
 *   the number of bytes that would have been stored if a buffer were
 *   provided.  So, to figure the size required, call this once with a zero
 *   buffer size, then allocate a buffer of (at least) that size and call
 *   again.
 */
int32 xtads_gameinfo_to_ifiction(int tads_version,
                                 const char *gi_txt, int32 gi_len,
                                 char *buf, int32 bufsize);

#endif /* TADS_H */
