/* advsys.c  Treaty of Babel module for AdvSys files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT advsys
#define HOME_PAGE "http://www.ifarchive.org/if-archive/programming/advsys/"
#define FORMAT_EXT ".dat"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 if (story_file || extent) { }
 ASSERT_OUTPUT_SIZE(8);
 strcpy(output,"ADVSYS-");
 return INCOMPLETE_REPLY_RV;

}

static int32 claim_story_file(void *story_file, int32 extent)
{
 char buf[7];
 int i;
 if (extent >=8)
 {
   for(i=0;i<6;i++)
    buf[i]=~(((char *)story_file)[i+2]+30);
   buf[6]=0;
   if (strcmp(buf,"ADVSYS")==0) return VALID_STORY_FILE_RV;
 }
 return INVALID_STORY_FILE_RV;
}
