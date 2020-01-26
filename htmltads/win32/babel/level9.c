/* level9.c  Treaty of Babel module for Level 9 files
 * 2006 By L. Ross Raszewski
 *
 * Note that this module will handle both bare Level 9 A-Code and
 * Spectrum .SNA snapshots.  It will not handle compressed .Z80 images.
 *
 * The Level 9 identification algorithm is based in part on the algorithm
 * used by Paul David Doherty's l9cut program.
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT level9
#define HOME_PAGE "http://www.if-legends.org/~l9memorial/html/home.html"
#define FORMAT_EXT ".sna,.l9"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>


static int32 read_l9_int(unsigned char *sf)
{
 return ((int32) sf[1]) << 8 | sf[0];

}
static int v2_recognition (unsigned char *sf, int32 extent)
{
  int32 i, l;
  for (i=0;i<extent-20;i++)
    if ((read_l9_int(sf+i+4) == 0x0020) &&
        (read_l9_int(sf+i+0x0a) == 0x8000) &&
        (read_l9_int(sf+i+0x14) == read_l9_int(sf+i+0x16)) &&
        ((l = read_l9_int(sf+i+0x1c)) != 0) &&
        ((l + i) <= extent))
             return 2;
 return 0;
}
static int v1_recognition(unsigned char *sf, int32 extent)
{
  int32 i;
  unsigned char a = 0xff, b = 0xff;

  for (i=0;i<extent-20;i++)
   if (memcmp(sf+i,"ATTAC",5)==0 && sf[i+5]==0xcb)
   {
    a = sf[i+6];
    break;
   }
  for (;i<(extent-20);i++)
   if (memcmp(sf+i,"BUNC",4)==0 && sf[i+4]==0xc8)
   {
    b = sf[i + 5];
    break;
   }
  if (a == 0xff && b == 0xff)
   return 0;
  return 1;
}
static int v3_recognition_phase (int phase,unsigned char *sf, int32 extent)
{
  int32 end, i, j, l, ll;
  ll=0;
  for (i=0;i<extent-20;i++)
  {
    if (ll) break;
    l = read_l9_int(sf+i);
    end=l+i;
    if (phase!=3)
    {
    if (end <= (extent - 2) &&
       (
        ((phase == 2) ||
        (((sf[end-1] == 0) &&
         (sf[end-2] == 0)) ||
        ((sf[end+1] == 0) &&
         (sf[end+2] == 0))))
        && (l>0x4000) && (l<=0xdb00)))
      if ((l!=0) && (sf[i+0x0d] == 0))
       for (j=i;j<i+16;j+=2)
        if (((read_l9_int(sf+j)+read_l9_int(sf+j+2))==read_l9_int(sf+j+4))
            && ((read_l9_int(sf+j)+read_l9_int(sf+j+2))))
        ll++;
     }
     else
     {
      if ((extent>0x0fd0) && (end <= (extent - 2)) &&
         (((read_l9_int(sf+i+2) + read_l9_int(sf+i+4))==read_l9_int(sf+i+6))
                    && (read_l9_int(sf+i+2) != 0) && (read_l9_int(sf+i+4)) != 0) &&
         (((read_l9_int(sf+i+6) + read_l9_int(sf+i+8)) == read_l9_int(sf+i+10))
          && ((sf[i + 18] == 0x2a) || (sf[i + 18] == 0x2c))
          && (sf[i + 19] == 0) && (sf[i + 20] == 0) && (sf[i + 21] == 0)))
        ll = 2;
     }
    if (ll>1)
     if (phase==3) ll=1;
     else
     { char checksum=0;
      for (j=i;j<=end;j++)
       checksum += sf[j];
      if (!checksum) ll=1;
      else ll=0;
     }
   }

  if (ll) return l < 0x8500 ? 3:4;
  return 0;
}

static int get_l9_version(unsigned char *sf, int32 extent)
{
 int i;
 if (v2_recognition(sf,extent)) return 2;
 i=v3_recognition_phase(1,sf,extent);
 if (i) return i;
 if (v1_recognition(sf,extent)) return 1;
 i=v3_recognition_phase(2,sf,extent);
 if (i) return i;
 return v3_recognition_phase(3,sf,extent);
}

static int32 claim_story_file(void *story, int32 extent)
{
 if (get_l9_version((unsigned char *) story,extent))
  return VALID_STORY_FILE_RV;
 return INVALID_STORY_FILE_RV;
}


static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 int i=get_l9_version((unsigned char *)story_file, extent);
 if (!i) return INVALID_STORY_FILE_RV;
 ASSERT_OUTPUT_SIZE(10);
 sprintf(output,"LEVEL9-%d-",i);
 return INCOMPLETE_REPLY_RV;
}
