/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadskb.h - TADS Windows Keyboard interface
Function
  This provides some services related to the Windows keyboard, particularly
  for generating and parsing the names of Windows virtual keys.
Notes

Modified
  10/27/06 MJRoberts  - Creation
*/

#ifndef TADSKB_H
#define TADSKB_H

#include "tadshtml.h"

/* shift key bits */
#define CTKB_SHIFT   0x0001
#define CTKB_CTRL    0x0002
#define CTKB_ALT     0x0004

/*
 *   keyboard utilities class
 */
class CTadsKeyboard
{
public:
    CTadsKeyboard();
    ~CTadsKeyboard();

    /* get the name of a key */
    int get_key_name(
        textchar_t *buf, size_t buflen, int vkey, int shiftkeys);

    /* parse a key name */
    int parse_key_name(
        const textchar_t **keykname, size_t *keylen,
        int *vkey, int *shiftkeys);

    /* canonicalize a key name */
    int canonicalize_key_name(textchar_t *keyname);

protected:
    /* hash tables to and from key names */
    class CHtmlHashTable *hash_keyname;
    class CHtmlHashTable *hash_vkey;

    /*
     *   Shifted key mapping.  For each virtual key K, this contains the
     *   character obtained from Shift+K, if any.
     */
    char shiftmap[256];
};

#endif /* TADSKB_H */
