# Scintilla

    Scintilla v3.0.2 | 2011/12/09

<!--  -->

- https://www.scintilla.org/
- https://sourceforge.net/projects/scintilla/files/scintilla/3.0.2/


-----

**Table of Contents**


<!-- MarkdownTOC autolink="true" bracket="round" autoanchor="false" lowercase="only_ascii" uri_encoding="true" levels="1,2,3,4" -->

- [Building Scintilla and SciTE](#building-scintilla-and-scite)
- [GTK+/Linux version](#gtklinux-version)
- [Windows version](#windows-version)
- [GTK+/Windows version](#gtkwindows-version)
- [License](#license)

<!-- /MarkdownTOC -->

-----


# Building Scintilla and SciTE

[`README`][README] for building of Scintilla and SciTE

Scintilla can be built by itself.

To build SciTE, Scintilla must first be built.

# GTK+/Linux version

You must first have GTK+ 2.0 or later and GCC (4.1 or better) installed.
GTK+ 1.x will not work.
Other C++ compilers may work but may require tweaking the make file.

To build Scintilla, use the [`makefile`][gtk make] located in the [`scintilla/gtk/`][gtk] directory

    cd scintilla/gtk
    make
    cd ../..

To build and install SciTE, use the `makefile` located in the `scite/gtk/` directory

    cd scite/gtk
    make
    make install

This installs SciTE into `$prefix/bin/`.
The value of `$prefix` is determined from the location of Gnome if it is installed.
This is usually `/usr` if installed with Linux or `/usr/local` if built from source.
If Gnome is not installed `/usr/bin` is used as the prefix.
The prefix can be overridden on the command line like `make prefix=/opt` but the same value should be used for both `make` and `make install` as this location is compiled into the executable.
The global properties file is installed at `$prefix/share/scite/SciTEGlobal.properties`.
The language specific properties files are also installed into this directory.

To remove SciTE

    make uninstall

To clean the object files which may be needed to change `$prefix`

    make clean

The current make file only supports static linking between SciTE and Scintilla.


# Windows version

A C++ compiler is required.
Visual Studio .NET 2010 is the development system used for most development although TDM Mingw32 4.4.1 is also supported.

To build Scintilla, make in the [`scintilla/win32/`][win32] directory

- `cd scintilla\win32`
- GCC: `mingw32-make`
- VS .NET: `nmake -f scintilla.mak`
- `cd ..\..`

To build SciTE, use the makefiles located in the `scite/win32/` directory

- `cd scite\win32`
- GCC: `mingw32-make`
- VS .NET: `nmake -f scite.mak`

An executable SciTE will now be in `scite\bin\`.

The Visual C++ 6.0 project (`.dsp`) and make files are no longer supported but are left in the download for people that are prepared to update them.

# GTK+/Windows version

Mingw32 is known to work.
Other compilers will probably not work.

Only Scintilla will build with GTK+ on Windows.
SciTE will not work.

To build Scintilla, make in the `scintilla/gtk/` directory

    cd scintilla\gtk
    mingw32-make


# License

- [`LICENSE`][LICENSE]

```
License for Scintilla and SciTE

Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>

All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.

NEIL HODGSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL NEIL HODGSON BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
OR PERFORMANCE OF THIS SOFTWARE.
```

<!-----------------------------------------------------------------------------
                               REFERENCE LINKS
------------------------------------------------------------------------------>

[LICENSE]: ./LICENSE "View license file"
[README]: ./README "View original README file"
[gtk]: ./gtk/ "Navigate to folder"
[win32]: ./win32/ "Navigate to folder"
[gtk make]: ./gtk/makefile "View makefile"

<!-- EOF -->
