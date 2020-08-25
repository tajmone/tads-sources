# TADS Sources ![build status][travis badge]


__TADS 2__, __TADS 3__ and __HTML TADS__ source files.

TADS (_Text Adventure Development System_) was created by Michael J. Roberts:

- [www.tads.org]

Repository created and maintained by [Tristano Ajmone], January 2020:

- https://github.com/tajmone/tads-sources

-----

**Table of Contents**

<!-- MarkdownTOC autolink="true" bracket="round" autoanchor="false" lowercase="only_ascii" uri_encoding="true" levels="1,2,3" -->

- [Project Structure](#project-structure)
- [Project Documentation](#project-documentation)
- [About](#about)
    - [Disclaimer](#disclaimer)
    - [Project Goals](#project-goals)
    - [Surviving the Death of 32-bit OSs](#surviving-the-death-of-32-bit-oss)
- [Upstream Sources](#upstream-sources)
- [Changes](#changes)
- [Contributing](#contributing)
- [List of TADS Files Types](#list-of-tads-files-types)
    - [TADS 2 Files Types by Extensions](#tads-2-files-types-by-extensions)
    - [TADS 3 Files Types by Extensions](#tads-3-files-types-by-extensions)
- [Licenses](#licenses)
    - [TADS 2 License](#tads-2-license)
    - [TADS 3 License](#tads-3-license)
    - [HTML TADS License](#html-tads-license)
    - [Scintilla License](#scintilla-license)
- [Links](#links)
    - [TADS Documentation](#tads-documentation)

<!-- /MarkdownTOC -->

-----

# Project Structure

- [`/htmltads/`](./htmltads/) — HTML TADS sources.
- [`/scintilla/`](./scintilla/) — [Scintilla] v3.0.2 sources for TADS Workbench.
- [`/t3doc/`](./t3doc/) — TADS 3 Documentation.
- [`/t3launch/`](./t3launch/) — php and MySQL scripts for setting up a TADS Web server.
- [`/tads2/`](./tads2/) — TADS 2 sources.
- [`/tads3/`](./tads3/) — TADS 3 generic sources.
- [`CHANGES.md`](./CHANGES.md) — list of all changes to the original files.

Repository settings:

- [`.editorconfig`](./.editorconfig) — [EditorConfig] settings for code styles.
- [`.gitattributes`](./.gitattributes) — EOL normalization rules and repo stats.
- [`.gitignore`](./.gitignore)
- [`.travis.yml`](./.travis.yml) — [Travis CI] settings for code styles validation.
- [`validate.sh`](./validate.sh) — code styles validation script.

# Project Documentation

Quick links to the documentation files of this repository.

To display documents in HTML format on GitHub, click on their ([Live HTML](#project-documentation "Live HTML Preview Link example")) link; if you're reading this document locally, just click on their file path.


<!-- MarkdownTOC:excluded -->
## HTML TADS

- [`htmltads/notes/changes.htm`][htmT/changes] — HTML TADS Changelog. ([Live HTML][htmT/changes Live])
- [`htmltads/notes/compat.htm`][htmT/compat] — System Compatibility Notes. ([Live HTML][htmT/compat Live])
- [`htmltads/notes/porting.htm`][htmT/porting] — Notes on porting HTML TADS. ([Live HTML][htmT/porting Live])

<!-- MarkdownTOC:excluded -->
## TADS 2

- [`tads2/README.md`][T2/README] — TADS 2 README.
- [`tads2/msdos/dosver.htm`][T2/MSDOS] — MS-DOS/Windows TADS 2 Changelog. ([Live HTML][T2/MSDOS Live])
- [`tads2/tadsver.htm`][T2/tadsver] — TADS 2 Changelog. ([Live HTML][T2/tadsver Live])
- [`tads2/unix/README.md`][T2/unix] — TADS 2 Unix Sources info.

<!-- MarkdownTOC:excluded -->
## TADS 3

- [`tads3/README.md`][T3/README] — TADS 3 README.
- [`t3doc/t3changes.htm`][T3/changes] — TADS 3 Changelog. ([Live HTML][T3/changes Live])
- [`tads3/charmap/README.md`][T3/charmap] — TADS Character Mapping Files.
- [`tads3/lib/adv3/changes.htm`][T3/adv3 changes] — TADS 3 Library Changelog. ([Live HTML][T3/adv3 changes Live])
- [`tads3/lib/extensions/TCommand/doc/tcommand.htm`][T3/TCommand] — __TCommand__ module documentation. ([Live HTML][T3/TCommand Live])
- [`tads3/portnote.htm`][T3/portnote] — Porting TADS 3. ([Live HTML][T3/portnote Live])
- [`tads3/test/README.md`][T3/test] — TADS 3 Test Suite.
- [`tads3/tz/db/README.md`][T3/tz/db] — IANA Time Zone Database.
- [`tads3/unix/README.md`][T3/unix] — TADS 3 Unix Sources info.
- [`tads3/unix/test/README.md`][T3/unix/test] — TADS 3 Unix Test Suite.

# About

This repository was created by [Tristano Ajmone] in January 2020.

## Disclaimer

This is my personal project and _it's not_ part of the official TADS project.
Although I've contacted via email TADS' author Michael J. Roberts before publishing this project (as requested in the TADS license terms) in order to inquire about its permissibility, and received his approval, this does not imply any official endorsement by TADS' author.

Furthermore, any accidental mistakes, inaccuracies or damages that might have occurred to the original sources and assets in this repository are my sole responsibility.

This repository _is not_ intended as a replacement of the official TADS sources available at www.tads.org or the [IF Archive], which remain the sole official distributions and should always be consulted and referred to.

## Project Goals

The main goals of this repository are:

- Gathering into a single repository all the TADS sources available from TADS website (and elsewhere).
- Enforce code styles consistency across source files to improve collaborative editing via Git.
- Port to [GitHub Flavored Markdown] all the documentation for better interoperability with GitHub.

All source code files and non-binary assets have been normalized to comply to strict code styles conventions (via [EditorConfig]).
These minor aesthetic tweaks ensure code style consistency across source files of the same type (indentation style, EOL, lack of trailing whitespace, etc.) without affecting code contents or any other meaningful aspect of the source assets.
Code consistency greatly improves the collaborative experience on version controlled projects, enforcing standards across different OSs and editors, thus ensuring a clean commit history, free of spurious contents changes relating to whitespace.

My motivation behind this work is to provide a Git compliant codebase of TADS sources that developers on GitHub can fork as a starting point to work efficiently with Git, GitHub and continuous integration services like [Travis CI] and others.

Hopefully this will serve not only archival purposes but also provide a solid code base for anyone wishing to maintain future TADS updates using Git and GitHub — the tools of choice for collaborative code editing those days.

## Surviving the Death of 32-bit OSs

Although all the tools in the TADS suite are mature and stable, we've recently witnessed an alarming trend to abandon 32-bit support on major OSs, which is putting at serious risks the Interactive Fiction legacy, where most of the tools have historically been released as 32-bit applications.

Ubuntu [officially announced in June 2019] that it would drop i386 releases and 32-bit support starting with Ubuntu 19.10.
It was only due to the [end users' and developers' outrage response], and [Steam announcing that it would in turn drop support for Ubuntu], that Canonical backtracked and decided to temporarily extend the "grace period" — but it's only a matter of time before this will come into effect, and other Linux distributions might follow that line too.

With the release of macOS Catalina, in October 2019, [Apple has officially dropped support for running 32-bit application on macOS], and removed all 32-bit apps from the Mac App Store.
Furthermore, macOS [Gatekeeper's increasingly stringent policies] are also interfering with many historical IF applications.

This war against 32-bit support on x86-64 OSs could have a devastating impact on the IF legacy tools, since it also affects emulators like Wine.
The risk is that we might witness a rapid shift toward the new generation of in-browser IF authoring tools (which seem to proliferate right now) at the expenses of the older (and in my view, _more robust_) historical tools that are so important for the IF cultural legacy.

The future of Interactive Fiction doesn't seem very bright at the moment, and unless we're prepared to revisit the sources of our beloved authoring tools in order to make them 64-bit compliant, they will soon become unusable (or too hard to use) on some of the major OSs — which amounts to a death sentence in the small niche of IF _connoisseurs_.

Hence the need to prepare the ground for new works on the sources of tools like TADS, by providing repositories which are strongly Git compliant and ready to be collaboratively edited on platforms like GitHub.
So, I hope that this effort of mine might contribute in the direction of preserving these old but beautiful IF authoring and playing tools — TADS 3 being one of the most powerful and elegant IF authoring tools ever created (if not _the_ most powerful and elegant tool).

# Upstream Sources

The above folders were extracted from the following ZIP files, downloaded from TADS website:

|    folder    | extracted from |  released  | version |       description        |
|--------------|----------------|------------|---------|--------------------------|
| `htmltads/`  | `htmltsrc.zip` | 2013/05/16 | 2.5.17  | HTML TADS  sources.      |
| `scintilla/` | `htmltsrc.zip` | 2013/05/16 | 3.0.2   | [Scintilla] sources.     |
| `t3doc/`     | `t3doc.zip`    | 2016/03/07 | 3.1.3   | TADS 3 Documentation.    |
| `t3launch/`  | `t3launch.zip` | 2016/03/08 | ---     | TADS Web server scripts. |
| `tads2/`     | `tads2src.zip` | 2013/05/16 | 2.5.17  | TADS 2 sources.          |
| `tads3/`     | `t3_src.zip`   | 2016/03/08 | 3.1.3   | TADS 3 generic sources.  |

Here are a few notes about the upstream sources, for completeness sake.

- With the exception of the `t3doc/` folder (originally named `doc/`), the above folders have the same names as found inside the ZIP archives.
- The `lib/` folder from the `t3doc.zip` archive was left out because it contained a single file (`lib/adv3/changes.htm`) which is already present inside `t3_src.zip` (`tads3/lib/adv3/changes.htm`), and the two files are identical.
- The archives `htmltsrc.zip` and `tads2src.zip` were downloaded from the _TADS 2 Source and Patches_ download page:
    + https://www.tads.org/t2_patch.htm
- The archives `t3doc.zip`, `t3launch.zip` and `t3_src.zip` were downloaded from the _À la carte components_ section at:
    + https://www.tads.org/tads3.htm
- The archives `adv3.zip` and `t3_hdr.zip` (available at the _À la carte components_ section) are not mentioned here because their contents are also found inside `t3_src.zip`, although organized in different folders structures.
Therefore, this repository _does_ contains all the source files available in all the ZIP archives from TADS website.

# Changes

The source files in this repository are faithful to the originals and didn't undergo any significant changes except for minor aesthetic tweaks for the sake of code styles consistency and better fruition of the project on GitHub.

These changes pertain mostly to code style consistency across files of the same type (EOLs, indentation style, trimming trailing spaces, etc.) and don't affect the source code in any meaningful way.

Some files where renamed for the sake of file naming consistency across the repository, adopting file names which are more in line with GitHub standards and practices:

- All license files were renamed as `LICENSE`.
- All plain-text README files were renamed as `README`.

Full detailed information on these changes can be found in a separate dedicated document:

- [`CHANGES.md`](./CHANGES.md "View the full list of changes")

# Contributing

Contributions are welcome as long as they don't alter the original code base.
The purpose of this repository is not updating TADS sources and assets but offering a polished code base of reference for projects that need to do so.
If you want to update the project files to improve the compilation process for a specific platform or compiler, just fork the repository, rename it as appropriate and go ahead (this is what's this repository is here for).

Areas of contributions include improving the additional markdown documents added to the original sources, in order to simplify navigation of the repository, cross referencing the various sub-projects, and better contextualize that various assets involved.

When submitting a pull request, just ensure that your submitted contents pass the [EditorConfig] validation test for code consistency.
Open Bash, cd to the repository root folder and type:

    ./validate.sh

To run the [`validate.sh`](./validate.sh) script you'll need to install Node.js and [EClint]:

    npm install -g eclint

Also, check that your editor/IDE supports [EditorConfig] either [natively or via a plug-in/package]; so you won't have to worry about code styles conventions when editing this project's files.

The [project Wiki] can be freely edited by anyone, and it's a good place for collecting useful notes on TADS sources, providing tutorials and build instructions, technical specifications and articles, as well as providing links to related projects and useful assets.

[Issues] can be used to ask questions and freely discuss ideas and proposals regarding TADS sources and were to go from here.


# List of TADS Files Types

The tables below illustrate all the file types associated with TADS 2 and TADS 3.

Please note that this repository does not contain sample files of every TADS file type (as reported in the __in-repo__ column), and some file types extensions are included for information purposes only.

Also note that some file extensions are common to both TADS 2 and TADS 3, and the tables below will report availability of sample files for each TADS version (e.g. there are no `.h` headers for TADS 2 in this repository, only for TADS 3).

Detailed info about these file types can be found in the _[TADS Media Types]_ article on TADS official website.

## TADS 2 Files Types by Extensions

|       ext.      |  type  |              description               | in-repo |
|-----------------|--------|----------------------------------------|---------|
| `*.gam`         | binary | TADS 2 compiled story file.            | _none_  |
| `*.h`           | text   | TADS 2/3 header file.                  | _none_  |
| `*.rs0`-`*.rs9` | binary | TADS 2 resource file.                  | _none_  |
| `*.sav`         | binary | TADS 2 saved game.                     | _none_  |
| `*.t`           | text   | TADS 2/3 source code file.             | yes     |
| `*.tcm`         | text   | TADS 2/3 compiled chars mapping table. | _none_  |
| `*.tcs`         | text   | TADS 2/3 source chars mapping table.   | yes     |

## TADS 3 Files Types by Extensions

|       ext.      |  type  |              description               | in-repo |
|-----------------|--------|----------------------------------------|---------|
| `*.3r0`-`*.3r9` | binary | TADS 3 resource file.                  | _none_  |
| `*.h`           | text   | TADS 2/3 header file.                  | yes     |
| `*.t3`          | binary | TADS 3 compiled story file.            | _none_  |
| `*.t3m`         | text   | TADS 3 project file.                   | yes     |
| `*.t3r`         | binary | TADS 3 resource file.                  | yes     |
| `*.t3tz`        | binary | TADS 3 Time-Zones related file.        | yes     |
| `*.t3v`         | binary | TADS 3 saved game.                     | _none_  |
| `*.t3x`         | binary | TADS 3 compiled story file.            | _none_  |
| `*.t`           | text   | TADS 2/3 source code file.             | yes     |
| `*.tcm`         | text   | TADS 2/3 compiled chars mapping table. | yes     |
| `*.tcs`         | text   | TADS 2/3 source chars mapping table.   | yes     |
| `*.tl`          | text   | TADS 3 library  file.                  | yes     |

<!--
| `*.XXXX` | ZZZ         |
-->

# Licenses

From [TADS website](https://www.tads.org/tads.htm):

> TADS is free software, with published source code.
> It has no ads, upgrade pitches, or other catches.
> TADS places no restrictions on the games you create with it; you're free to give them away, distribute them as shareware, sell them commercially, or do anything else you want with them.
> The TADS interpreter can be freely redistributed with your game, even if you publish your game commercially.

## TADS 2 License

- [`tads2/LICENSE`](./tads2/LICENSE "View the full license text")

```
TADS 2 FREEWARE SOURCE CODE LICENSE

The TADS 2 source code is Copyright 1991, 2003 by Michael J. Roberts.

The author hereby grants you permission to use, copy, and
distribute this software, if you agree to the following conditions:

    1. You must include this license and the copyright notice with
       all copies.
    2. You may not require or collect a fee for copies of this
       software, or any part of this software, that you give to
       other people.
    3. You may not include this software with any other software
       for which a fee is collected.
    4. You may not modify this software except as permitted below
       (see "derivative works"), and each copy you make and
       distribute must be a full and complete copy of the software
       you originally received.
    5. Anyone to whom you give a copy of this software receives
       all of the same permissions that you did under this license
       and is subject to all of the same restrictions.
    6. You are not allowed to create derivative works, which are
       works that contain or are based on all or part of this work,
       except under the conditions described below.
    7. Any derivative works are subject to this same license.


Derivative Works
----------------

This source code is distributed for the specific purpose of
facilitating the creation of versions of TADS on various computers and
operating systems.  All other derivative works are prohibited without
the written permission of the author.  Please contact the author if
you have any questions about this or if you'd like permission to
create a derived work.

If you port TADS to a new platform, the author does grant permission
for you to distribute your ported version - I encourage it, in fact.
We ask that you provide your contact information in any distribution
package you create, so that users of your version will know how to
contact you if they have any questions relating specifically to your
version.
```

## TADS 3 License

- [`tads3/LICENSE`](./tads3/LICENSE "View the full license text")

```
TADS 3 FREEWARE SOURCE CODE LICENSE

The TADS 3 source code is Copyright 1998, 2012 by Michael J. Roberts.

The author hereby grants you permission to use, copy, and distribute
this software, if you agree to the following conditions:

    1. You must include this license and the copyright notice with
       all copies.
    2. You may not require or collect a fee for copies of this
       software, or any part of this software, that you give to
       other people.
    3. You may not include this software with any other software
       for which a fee is collected.
    4. You may not modify this software except as permitted below
       (see "derivative works"), and each copy you make and
       distribute must be a full and complete copy of the software
       you originally received.
    5. Anyone to whom you give a copy of this software receives
       all of the same permissions that you did under this license
       and is subject to all of the same restrictions.
    6. You are not allowed to create derivative works, which are
       works that contain or are based on all or part of this work,
       except under the conditions described below.
    7. Any derivative works are subject to this same license.


Derivative Works
----------------

This source code is distributed for the specific purpose of porting
TADS, so that you can run the software on any system of your choosing.
All other derivative works are prohibited without the written
permission of the author.  I want to avoid the creation of variations
on the system, because it leads to confusion on the part of users if
there are multiple incompatible flavors floating around.  However, if
you have a specific idea in mind, I'd be happy to at least consider
it.  Please contact the author if you have any questions about this or
if you would like permission to create a derived work.

If you port TADS to a new platform, the author does grant permission
for you to distribute your ported version - I encourage it, in fact.
I ask that you include your contact information in any distribution
package you create, so that users of your version will know how to
contact you if they have any questions relating specifically to your
version.
```

## HTML TADS License

- [`htmltads/LICENSE`](./htmltads/LICENSE "View the full license text")

```
HTML TADS FREEWARE SOURCE CODE LICENSE

The HTML TADS source code is Copyright 1997, 2013 by Michael J. Roberts.

The author hereby grants you permission to use, copy, and
distribute this software, if you agree to the following conditions:

    1. You must include this license and the copyright notice with
       all copies.
    2. You may not require or collect a fee for copies of this
       software, or any part of this software, that you give to
       other people.
    3. You may not include this software with any other software
       for which a fee is collected.
    4. You may not modify this software except as permitted below
       (see "derivative works"), and each copy you make and
       distribute must be a full and complete copy of the software
       you originally received.
    5. Anyone to whom you give a copy of this software receives
       all of the same permissions that you did under this license
       and is subject to all of the same restrictions.
    6. You are not allowed to create derivative works, which are
       works that contain or are based on all or part of this work,
       except under the conditions described below.
    7. Any derivative works are subject to this same license.


Derivative Works
----------------

This source code is distributed for the specific purpose of
facilitating the creation of versions of TADS on various computers and
operating systems.  All other derivative works are prohibited without
the written permission of the author.  Please contact the author if
you have any questions about this or if you'd like permission to
create a derived work.

If you port TADS to a new platform, the author does grant permission
for you to distribute your ported version - I encourage it, in fact.
We ask that you provide your contact information in any distribution
package you create, so that users of your version will know how to
contact you if they have any questions relating specifically to your
version.
```

## Scintilla License

- [`scintilla/LICENSE`](./scintilla/LICENSE "View the full license text")

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

# Links

- [www.tads.org]
- [IFWiki » TADS]
- [IFWiki » TADS 3]
- [Wikipedia » TADS]

## TADS Documentation

- [TADS Media Types] — by Andreas Sewe.
- [TADS Tools Overview]
- [TADS Web Play Overview]
- [TADS 2 Bookshelf]
    + _[The TADS 2 Author's Manual]_ — by Michael J. Roberts and Neil K. Guy.
    + _[The TADS Parser Manual]_ — by Michael J. Roberts.
- [TADS 3 Bookshelf]
    + _[Getting Started in TADS 3]_ — by Eric Eve.
    + _[Introduction to HTML TADS]_ — by Michael J. Roberts.
    + _[Learning TADS 3]_ (local PDF) — by Eric Eve.
    + _[TADS 3 Library Reference Manual]_ — by Michael J. Roberts.
    + _[TADS 3 Quick Start Guide]_ — by Eric Eve.
    + _[TADS 3 System Manual]_ — by Michael J. Roberts.
    + _[TADS 3 Technical Manual]_ — by Steve Breslin, Eric Eve and Michel Nizette,
        * [T3 Virtual Machine Technical Documentation]
    + _[TADS 3 Tour Guide]_ — by Eric Eve.
Michael J. Roberts and Andreas Sewe.

<!-----------------------------------------------------------------------------
                               REFERENCE LINKS
------------------------------------------------------------------------------>

[GitHub Flavored Markdown]: https://github.github.com/gfm/#what-is-github-flavored-markdown- "Learn more about GitHub Flavored Markdown"
[IF Archive]: https://www.ifarchive.org/ "Visit the IF Archive website"
[natively or via a plug-in/package]: https://editorconfig.org/#download

<!-- TADS -->

[www.tads.org]: https://www.tads.org/ "Visit TADS official website"

[IFWiki » TADS]: http://ifwiki.org/index.php/TADS "See the TADS page on the IFWiki"
[IFWiki » TADS 3]: http://ifwiki.org/index.php/TADS_3 "See the TADS 3 page on the IFWiki"
[Wikipedia » TADS]: https://en.wikipedia.org/wiki/TADS "See the TADS page on Wikipedia"

<!-- TADS official documentation & articles -->

[Introduction to HTML TADS]: https://www.tads.org/t3doc/doc/htmltads/intro.htm "Read the article on TADS website"
[TADS Media Types]: https://www.tads.org/howto/mediatypes.htm "Read the 'TADS Media Types' technical article on TADS website"
[TADS Tools Overview]: https://www.tads.org/ov_tools.htm "Read the 'TADS Overview: Tools' article on TADS website"
[TADS Web Play Overview]: https://www.tads.org/ov_web.htm "Read the 'TADS Overview: Web Play' article on TADS website"

[TADS 2 Bookshelf]: https://www.tads.org/t2doc/index.htm "Visit the TADS 2 Bookshelf at www.tads.org"
[The TADS Parser Manual]: https://www.tads.org/t2doc/doc/parser.htm "Read 'The TADS Parser Manual' on-line at TADS website"
[The TADS 2 Author's Manual]: https://www.tads.org/t2doc/doc/index.html "Read 'The TADS 2 Author's Manual' on-line at TADS website"

[TADS 3 Bookshelf]: https://www.tads.org/t3doc/doc/ "Visit the TADS 3 Bookshelf at www.tads.org"
[TADS 3 Tour Guide]: https://www.tads.org/t3doc/doc/tourguide/index.html "Read 'TADS 3 Tour Guide' on-line at TADS website"
[Getting Started in TADS 3]: https://www.tads.org/t3doc/doc/gsg/index.html "Read 'Getting Started in TADS 3' on-line at TADS website"
[Learning TADS 3]: ./t3doc/learning/Learning%20T3.pdf "View the PDF document in this repository"
[T3 Virtual Machine Technical Documentation]: https://www.tads.org/t3doc/doc/techman/t3spec.htm "Learn more about the T3 VM on TADS website"
[TADS 3 Library Reference Manual]: https://www.tads.org/t3doc/doc/libref/index.html  "Read 'TADS 3 Library Reference Manual' on-line at TADS website"
[TADS 3 Quick Start Guide]: https://www.tads.org/t3doc/doc/t3QuickStart.htm "Read the 'TADS 3 Quick Start Guide' on-line at TADS website"
[TADS 3 System Manual]: https://www.tads.org/t3doc/doc/sysman/cover.htm "Read 'TADS 3 System Manual' on-line at TADS website"
[TADS 3 Technical Manual]: https://www.tads.org/t3doc/doc/techman/cover.htm "Read 'TADS 3 Technical Manual' on-line at TADS website"

<!-- 3rd party tools -->

[EClint]: https://www.npmjs.com/package/eclint "Visit EClint page at NPM"
[EditorConfig]: https://editorconfig.org/ "Visit EditorConfig website"
[Scintilla]: https://www.scintilla.org/ "Visit Scintilla website"
[Travis CI]: https://travis-ci.com/ "Visit Travis CI website"

<!-- 32-bits support -->

[Apple has officially dropped support for running 32-bit application on macOS]: https://www.cnet.com/how-to/macos-catalina-is-killing-off-32-bit-apps-heres-what-that-means-for-you/ "Read the article 'MacOS Catalina is killing off 32-bit apps. Here’s what that means for you' on CNET"
[end users' and developers' outrage response]: https://www.engadget.com/2019/06/24/canonical-ubuntu-linux-32-bit-support/ "Read the article 'Canonical backtracks on pulling 32-bit support from Ubuntu Linux' by endgadget.com"
[Gatekeeper's increasingly stringent policies]: http://inform7.com/releases/2019/10/10/catalina.html "Learn more about his topic on Inform 7 new on macOS Catlina"
[officially announced in June 2019]: https://lists.ubuntu.com/archives/ubuntu-announce/2019-June/000245.html "Read the announcement at the Ubuntu archives"
[Steam announcing that it would in turn drop support for Ubuntu]: https://www.engadget.com/2019/06/22/steam-will-stop-supporting-ubuntu-linux/ "Read the article 'Steam will stop supporting Ubuntu Linux over 32-bit compatibility' by endgadget.com"

<!-- repo links -->

[Issues]: https://github.com/tajmone/tads-sources/issues "View the project's Issues"
[project Wiki]: https://github.com/tajmone/tads-sources/wiki "Visit the Wiki of this repository"

<!-- markdown documentation files -->

[T2/README]: ./tads2/README.md
[T2/unix]: ./tads2/unix/README.md
[T3/README]: ./tads3/README.md
[T3/charmap]: ./tads3/charmap/README.md
[T3/test]: ./tads3/test/README.md
[T3/tz/db]: ./tads3/tz/db/README.md
[T3/unix]: ./tads3/unix/README.md
[T3/unix/test]: ./tads3/unix/test/README.md

<!-- HTML documentation files -->

[htmT/changes]: ./htmltads/notes/changes.htm
[htmT/compat]: ./htmltads/notes/compat.htm
[htmT/porting]: ./htmltads/notes/porting.htm

[T2/MSDOS]: ./tads2/msdos/dosver.htm
[T2/tadsver]: ./tads2/tadsver.htm

[T3/changes]: ./t3doc/t3changes.htm
[T3/adv3 changes]: ./tads3/lib/adv3/changes.htm
[T3/TCommand]: ./tads3/lib/extensions/TCommand/doc/tcommand.htm
[T3/portnote]: ./tads3/portnote.htm

<!-- HTML documentation Live HTML links -->

[htmT/changes Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/htmltads/notes/changes.htm "Live HTML Preview link"
[htmT/compat Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/htmltads/notes/compat.htm "Live HTML Preview link"
[htmT/porting Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/htmltads/notes/porting.htm "Live HTML Preview link"

[T2/MSDOS Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/tads2/msdos/dosver.htm "Live HTML Preview link"
[T2/tadsver Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/tads2/tadsver.htm "Live HTML Preview link"

[T3/changes Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/t3doc/t3changes.htm "Live HTML Preview link"
[T3/adv3 changes Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/tads3/lib/adv3/changes.htm "Live HTML Preview link"
[T3/TCommand Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/tads3/lib/extensions/TCommand/doc/tcommand.htm "Live HTML Preview link"
[T3/portnote Live]: https://htmlpreview.github.io/?https://github.com/tajmone/tads-sources/blob/master/tads3/portnote.htm "Live HTML Preview link"

<!-- badges -->

[travis badge]: https://travis-ci.com/tajmone/tads-sources.svg?branch=master
 "Travis CI Build Status for EditorConfig Validation"

<!-- people -->

[Tristano Ajmone]: https://github.com/tajmone "View Tristano Ajmone's GitHub profile"

<!-- EOF -->
