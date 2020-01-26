# Changes List

This document provides detailed information on all the changes that were done in this repository to the original upstream files.

All these changes are merely aesthetic, the original source code has not undergone any meaningful changes.

> **WARNING** — This is a rather long and boring document, written out of duty and provided mainly for the benefit of historical preservation accuracy.
> Chances are that you don't need to read any of this, unless you have good reasons to do so.
> Life is too short, instead of reading this boring stuff you should invest your time creating a beautiful text adventure with TADS.

-----

**Table of Contents**

<!-- MarkdownTOC autolink="true" bracket="round" autoanchor="false" lowercase="only_ascii" uri_encoding="true" levels="1,2,3" -->

- [Premise](#premise)
- [List of Changes](#list-of-changes)
    - [Renamed Files](#renamed-files)
    - [Deleted Files](#deleted-files)
    - [Added Files](#added-files)
    - [Code Style Consistency Tweaks](#code-style-consistency-tweaks)
        - [TADS 2/3 Sources](#tads-23-sources)
        - [TADS Character Mapping Files](#tads-character-mapping-files)
        - [TADS C/C++ Sources](#tads-cc-sources)
        - [Scintilla C/C++ Sources](#scintilla-cc-sources)
        - [Makefiles](#makefiles)
        - [Shell Scripts](#shell-scripts)
        - [HTML and CSS Files](#html-and-css-files)

<!-- /MarkdownTOC -->

-----

# Premise

The source files in this repository are faithful to the originals and didn't undergo any significant changes except for minor aesthetic tweaks and adaptations for the sake of code styles consistency and better fruition of the project via Git and GitHub.

But since some files were edited nonetheless, I feel it's my duty to document these changes — no matter how small or aesthetic they might be.

First of all, I'd like to emphasise the importance of these small file tweaks, why they do not affect the original sources and why they are needed.

> **WARNING** — If you're the "batteries included" lover type — who doesn't care how things work behind the scene, as long as they're "auto-magically" usable out of the box — then you might find the rest of this section rather boring and might just as well [skip directly to changes list].
>
> On the other hand, if you're the seeker type — who always loves to investigate how the magic behind all "auto-magic" things works — you might find the following details rather useful, especially if you often deal with legacy-code revival projects.

All the TADS sources were taken from the official ZIP archives downloaded from www.tads.org.
These files include TADS 2 sources, some of which could possibly date back as far as 1996 (or even older, in case some of them were re-adapted from TADS 1).
Being very old files, which have been re-worked across multiple OSs, and for so many years, it would be unreasonable to expect to just dump them all together into a Git repository as they are and expect that everything is going to work out fine.

First of all, we must consider possible encoding issues, since we're dealing with tools that flourished in the pre-Unicode era, when the norm was to work with CodePages, ASCII and ISO encodings.
Although TADS 3 was already offering Unicode support, its usage was the exception rather than the norm, and back then most editors and command line tools were unable to handle properly Unicode encoding.
Today we're witnessing the opposite situation, with UTF-8 files being the norm, and many editors and command line tools having trouble to handle legacy encodings (often corrupting ISO encoded files on the assumption they are UTF-8 files).

Then we need to consider the fact that in this repository we're grouping the contents from different ZIP archives, each one being an independent project whose contents don't necessary follow the same conventions as the others.
Also, the original TADS source files from the ZIP archives were not version controlled, with the exception of the Scintilla sources (which required more adjustments than the TADS sources did).

The requirements and standards for locally maintained code and version controlled public repositories for collaborative editing are quite different, hence the need for these changes in order to make the transition smooth — which, I hope should clarify that there wasn't indeed anything wrong in the original files, the problem being due to shift of working context and tools instead.

When publishing legacy code and assets on GitHub, issues like encodings need to be considered carefully (Git doesn't offer a great deal of settings for preserving the integrity of legacy encoded files), especially if these files are going to be edited by a multitude of end users working on different OSs, editors and IDEs (each one being biased in its own way when it comes to file standards).

Meet [EditorConfig]: a formal specification to help preserve code-styles consistency for multiple developers working on the same project across various editors and IDEs.
Thanks to EditorConfig settings ([`.editorconfig`][.editorconfig]) this project sets some code style standards for every file type, to ensure that similar files will display consistent indentation rules (tabs vs spaces, and width consistency), correct file encoding, and even prevent trailing whitespace and enforce an empty end line.

Combining EditorConfig settings with [`.gitattributes`][.gitattributes] rules for [Git EOL normalization]  (i.e. native, `LF` or `CRLF`) is a great way to ensure not only that the initial repository contents are well formatted to start with, but also allows to enforce these standards in contributors' commits and pull requests via validation tools like [EClint], which can be exploited through Continuous Integration services like [Travis CI] to ensure that all submitted contents meet the required styling standards — which is exactly what this repository does via these files:

- [`.travis.yml`][.travis.yml] — Travis CI configuration.
- [`validate.sh`][validate.sh] — EditorConfig validation script.

Furthermore, most modern editors should be able to automatically pick-up the EditorConfig settings from the repository root and enforce them at save time, overriding any custom settings in favour of project-specific settings.
Therefore, the mere addition of EditorConfig settings to a repository should suffice to transparently ensure that most contributors will be able to abide to the required code style convention, without requiring any active efforts on their side.

Unless you've experienced in first person the horrors of unkempt and ruleless collaborative projects, where every and each contributor keeps enforcing his/her own code styles on each committed hunk, you probably can't appreciate the importance of enforcing code styles consistency.
Not unless you've battled with kaleidoscopic source files where different EOL types are freely mixed in the same files, along with tabs and spaces indentations of all imaginable widths, and where leaving behind trailing whitespace is considered acceptable and harmless.
Not until you've experienced having to dig through gigantic commits diffs, where 90% of the changes are just useless noise due to spurious whitespace, EOL conversions, and all tabs being turned into spaces (and vice versa) by some smart editor obeying its own solitary rules.
Not until you've lived on your skin the post-traumatic symptoms of a full fledged code-styles war and its escalation of contributors fighting over the same files, overriding each other's automatic editor fixes to the code, in turn, commit after commit, in a futile competition that could have been avoided by enforcing styles rules in the first place.

If you want a project to truly work out of the box, then you need to _think_ out of the box — and to learn from past mistakes, and adopt the right tools and measures accordingly.

Although this project _was not_ designed with further development of TADS in mind, but rather to preserve all TADS sources under a single umbrella repository, these code styles adjustments and safeguards _were_ designed to aid those who do wish to further work on/with TADS sources — i.e. by forking or cloning this repository.

So, ultimately, all these consistency tweaks and rules are indeed aimed at preserving the integrity of these sources in view of future development, with an eye on cross-platform collaboration.
I'm confident that, for anyone wishing to take on TADS development on GitHub,
the sources of this repository are a better starting point that the official ZIP files.

# List of Changes

## Renamed Files

Some files where renamed for the sake of file naming consistency across the repository, adopting file names which are more in line with GitHub standards:

- All license files were renamed as `LICENSE`.
- All plain-text README files were renamed as `README`.

The table below provides detailed information on all renamed files.

|          original name          |             renamed             |
|---------------------------------|---------------------------------|
| `htmltads/LICENSE.TXT`          | `htmltads/LICENSE`              |
| `htmltads/win32/mpegamp/readme` | `htmltads/win32/mpegamp/README` |
| `scintilla/License.txt`         | `scintilla/LICENSE`             |
| `tads2/LICENSE.TXT`             | `tads2/LICENSE`                 |
| `tads3/charmap/README.txt`      | `tads3/charmap/README`          |
| `tads3/LICENSE.TXT`             | `tads3/LICENSE`                 |
| `tads3/README.txt`              | `tads3/README`                  |
| `tads3/test/readme.txt`         | `tads3/test/README`             |

<!--
| `xxxx` | `xxxx`
-->

## Deleted Files

Some no-longer needed file were deleted — e.g., [Mercurial] repository settings from the Scintilla folder.

|         delete files         |    description     |
|------------------------------|--------------------|
| `scintilla/.hg_archival.txt` | Mercurial settings |
| `scintilla/.hgeol`           | Mercurial settings |
| `scintilla/.hgignore`        | Mercurial settings |
| `scintilla/.hgtags`          | Mercurial settings |

## Added Files

To improve the GitHub experience, some extra files were added.
Besides some Git/GitHub specific settings files, markdown versions of the original plain-text README documents were added (leaving the originals in place, albeit renamed).

The converted markdown files were slightly edited in order to mirror the current state of the repository, e.g. by adding cross reference links, and were reformatted to provide a better reading experience.
The original text was edited as required, e.g. in case of dead links or obsolete information, and extra contents were added to make contents discovery and navigation of the repository easier.

Hopefully, all of these changes are in line with the intentions of the original author, and availability of the original and unedited plaint text documents should dispel any doubts regarding what was added or changed.

|        added/converted file        |             original            |        changes         |
|------------------------------------|---------------------------------|------------------------|
| `.editorconfig`                    | _none_                          | ---                    |
| `.gitattributes`                   | _none_                          | ---                    |
| `.gitignore`                       | _none_                          | ---                    |
| `.travis.yml`                      | _none_                          | ---                    |
| `CHANGES.md`                       | _none_                          | ---                    |
| `htmltads/win32/mpegamp/README.md` | `htmltads/win32/mpegamp/readme` | Markdown adaptation    |
| `README.md`                        | _none_                          | ---                    |
| `scintilla/.gitignore`             | `scintilla/.hgignore`           | Adapted from GH to Git |
| `scintilla/README.md`              | `scintilla/README`              | Markdown adaptation    |
| `tads2/README.md`                  | `readtads.src`                  | Markdown adaptation    |
| `tads2/unix/README.md`             | `tads2/unix/README`             | Contents updated       |
| `tads3/charmap/README.md`          | `tads3/charmap/README.txt`      | Markdown adaptation    |
| `tads3/README.md`                  | `tads3/README.txt`              | Markdown adaptation    |
| `tads3/test/README.md`             | `tads3/test/readme.txt`         | Markdown adaptation    |
| `tads3/tz/db/README.md`            | `tads3/tz/db/README`            | Markdown adaptation    |
| `tads3/unix/README.md`             | `tads3/unix/README`             | Markdown adaptation    |
| `tads3/unix/test/README.md`        | `tads3/unix/test/README`        | Markdown adaptation    |
| `validate.sh`                      | _none_                          | ---                    |


## Code Style Consistency Tweaks

Every non-binary file extensions as been covered in the repository settings to ensure consistent end-of-line normalization across different OSs (via [`.gitattributes`][.gitattributes]) as well as to enforce some basic code style conventions across similar file types (via [`.editorconfig`][.editorconfig]) like indentation rules, file encoding, by trimming trailing whitespace and adding or not an empty line at the end of the file.

As a general rule, all text source files (of all types and kinds) where stripped of trailing whitespace, an empty line at the end of the file was added (if not present), and standard indentation was enforced (either spaces or tabs, according to needs) — this document won't cover all of the file types that were subjected to these code style consistency rules and changes, but only focus on the main files types of interest in relation to the TADS project (the full details can be viewed in the [`.editorconfig`][.editorconfig] file).

These changes are aimed at providing a better user experience when editing code, allowing code consistency of same file types and across different editors/IDEs and OSs; they also improve the quality of commits, avoiding spurious whitespace from creeping into changes diffs and polluting the repository history with non-meaningful changes, which make it harder to focus on the actual code changes.

None of these changes affects the functionality of the source files, and the benefits of having strict code style consistency rules outweigh any aesthetic changes or losses (e.g. _ad hoc_ custom alignments in make files by mixing tabs and spaces), for these EditorConfig rules will allow modern editors to abide by these standards in any future edits, as well as providing the means to validate code style consistency in future contributions via continuous integration services like Travis CI, which can test whether any commit or pull request abide by these consistency rules.

### TADS 2/3 Sources

- Extensions: `*.t`, `*.T`, `*.tl`, `*.t3m`, `*.tdb-project-starter`.

All TADS library sources and adventures samples were trimmed of trailing whitespace, and all tabs indentations were converted to spaces on a per-source basis to ensure correct alignment preservation.

Most TADS sources are in ASCII encoding, with a few exceptions in the `tads3/test/data/` tests folder, which were designed specifically to test specific encodings:

|         source file          |  encoding | EditorConfig |
|------------------------------|-----------|--------------|
| `tads3/test/data/ucs2_src.t` | UCS-2     | _skipped_    |
| `tads3/test/data/utf-16be.t` | UTF-16 BE | _skipped_    |
| `tads3/test/data/utf-16le.t` | UTF-16 LE | _skipped_    |
| `tads3/test/data/utf-8.t`    | UTF-8 BOM | `utf-8`      |

Special exceptions rules where made in `.editorconfig` for these files; some of them being skipped from validation due to the [EClint] validation tool being unable to handle properly the files (chances are that the encoding of files `ucs2_src.t`, `utf-16le.t` and  `utf-16be.t` was corrupted somewhere along the line, for they're being detected as UTF-8 BOM).

EditorConfig doesn't provide specific support for ASCII and ISO encodings, so the default settings for TADS sources validation is UTF-8 (without BOM), which should work fine with US-ASCII encoded files (although it would fail reporting encoding breakage if Unicode characters were to be inserted).

### TADS Character Mapping Files

- Extensions: `*.tcs`

Some `*.tcs` files contained a spurious binary 0x1A character (usually at line 275), which interfered with the EOL normalization process, and could mislead Git into detecting the text file as a binary file.
The spurious character was removed.

Presence of a 0x1A char is a known side effect from old MS DOS multi-file operations (e.g. concatenations) where one or more text files where being treated as binary files.


### TADS C/C++ Sources

- Extensions: `*.c`, `*.C`, `*.cpp`, `*.CPP`, `*.h`, `*.H`.

The TADS C/C++ sources were trimmed of all trailing whitespace and indented using spaces only.
None of these changes affected the original alignments in the source files, which was preserved intact.

### Scintilla C/C++ Sources

- Extensions: `*.cxx*`, `*.m`, `*.mm`.

The Scintilla sources were subjected to the same rules as the TADS C/C++ sources, but manual interventions were required to try and preserve the original indentation since different files used different indentation rules and often mixed spaces and tabs in the same source (this messy state of the sources being probably the result of many different coders working on the sources with different editors and custom settings).

As a result, the tweaked Scintilla sources don't follow a strict indentation protocol — some files adopt two spaces as indentation, others three or four spaces — and the new spaces-only indentation might not be very consistent despite my best efforts to manually tweak the source files (but neither were the originals, for that matter).

Since the Scintilla sources are an auxiliary asset for projects like the TADS Workbench, these aesthetic differences shouldn't have any impact on the TADS sources preservation aspect of this project.

### Makefiles

- Extensions: `*.mak`, `*.mk`, `makefile`, `make-*.*`, `Makefile`, `MAKEFILE`, `makefile*.*`.

On all makefiles I've enforced Unix style EOLs (`LF`) and tabs indentation, which is regarded as best practice for cross platform development.

In some makefiles, these tweaks have affected the aesthetics of arbitrary indentation alignments (which relied on a mixture of tabs and spaces, or spaces only) and were hereby often replaced with a single tab indentation, for the sake of passing automated EditorConfig validation (and lacking the possibility of preserving the intended indentation via single spaces, due to the new code style convention being enforced).

As an example of how these changes might have impacted aesthetics, a makefile that was previously manually aligned like this:

```makefile
OBJS = tadshtml.obj htmlprs.obj htmltags.obj htmlfmt.obj htmldisp.obj \
       htmlsys.obj htmlw32.obj tadswin.obj tadsapp.obj tadsfont.obj \
       htmltxar.obj htmlinp.obj htmljpeg.obj htmlrc.obj htmlhash.obj \
       tadsstat.obj htmlpref.obj tadsdlg.obj htmlplst.obj tadsreg.obj \
       oshtml.obj tadsjpeg.obj htmlpng.obj tadsimg.obj tadspng.obj \
       htmlsnd.obj tadssnd.obj tadsmidi.obj tadswav.obj hos_w32.obj
```

now looks something like this:

```makefile
OBJS = tadshtml.obj htmlprs.obj htmltags.obj htmlfmt.obj htmldisp.obj \
    htmlsys.obj htmlw32.obj tadswin.obj tadsapp.obj tadsfont.obj \
    htmltxar.obj htmlinp.obj htmljpeg.obj htmlrc.obj htmlhash.obj \
    tadsstat.obj htmlpref.obj tadsdlg.obj htmlplst.obj tadsreg.obj \
    oshtml.obj tadsjpeg.obj htmlpng.obj tadsimg.obj tadspng.obj \
    htmlsnd.obj tadssnd.obj tadsmidi.obj tadswav.obj hos_w32.obj
```

Similar changes will not affect in any way the usability of these makefiles, but might slightly impact their readability.

### Shell Scripts

- Extensions:
    + `*.sh`
    + `/tads3/unix/test/test_*`
    + `/tads3/unix/test/run_*`
    + `scintilla/tgzsrc` (in Scintilla sources)
    + `*.iface` (in Scintilla sources)

For shell scripts, tab-indentation and Unix-style `LF` EOLs were enforced, which required tweaking the spaces-indentation of some TADS shell scripts.

### HTML and CSS Files

For CSS files, 2-spaces indentation was enforced.

In HTML files (including auto-generated ones) trailing whitespace was trimmed and all tab indentations were converted to spaces (in a few cases, manually enforcing consistency, e.g. where some documents used 3-spaces along with 4-spaces indentation).

Some HTML files still used the archaic Mac EOL (`CR`) and had to be manually normalized to native EOLs in order to be properly handled by the EClint validator tool.

Some HTML files rely on old CodePage encodings (possibly required in order to correctly visualize the HTML pages in the WebBrower Control), so their encoding was left untouched.

JavaScript files, as well as some other file types related to the TADS Web Server were handled with best practice code style rules, requiring small tweaks (if any).

-------------------------------------------------------------------------------

[Tristano Ajmone], January 2020, Italy.

<!-----------------------------------------------------------------------------
                               REFERENCE LINKS
------------------------------------------------------------------------------>

<!-- 3rd party tools -->

[Mercurial]: https://www.mercurial-scm.org/ "Visit Mercurial website"
[Scintilla]: https://www.scintilla.org/ "Visit Scintilla website"
[EditorConfig]: https://editorconfig.org/ "Visit EditorConfig website"
[EClint]: https://www.npmjs.com/package/eclint "Visit EClint page at NPM"
[Travis CI]: https://travis-ci.com/ "Visit Travis CI website"

<!-- articles -->

[Git EOL normalization]: https://adaptivepatchwork.com/2012/03/01/mind-the-end-of-your-line/ "Read Tim Clem's article 'Mind the End of Your Line' to learn more about Git EOL normalization"

<!-- xrefs -->

[skip directly to changes list]: #list-of-changes "Skip this section and jump straight to the list of changes"

<!-- repo files -->

[.editorconfig]: ./.editorconfig "View the EditorConfig settings"
[.gitattributes]: ./.gitattributes "View the .gitattributes settings"
[.travis.yml]: ./.travis.yml "View the Travis CI settings"
[validate.sh]: ./validate.sh "View the code styles validation script"

<!-- people -->

[Tristano Ajmone]: https://github.com/tajmone "View Tristano Ajmone's GitHub profile"

<!-- EOF -->
