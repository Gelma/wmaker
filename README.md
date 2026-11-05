# What's this?

**Short answer:** AI-Powered [Window Maker](https://www.windowmaker.org)

Well, let's start from the beginning.

[Window Maker](https://www.windowmaker.org) is my every-day laptop work desktop,
since 1998 (yes, you read right, it was last century!).

I am sincerely grateful to all the developers who,
over time, have contributed to and continuously
improved what, for my own use, has always been the
most convenient and productive graphical environment.

I'm not such a good programmer, so I've never been able
to contribute, but - nowadays - my contribution is
about telling AI what to do, in which way, and
much more (much more than this...) test it extensively
because - again - I always use [WMaker](https://www.windowmaker.org), every day
(and more than this... long sessione, seldom laptop reboot, usually suspend/resume for
weeks).

So, what's this?

Well, these are the commits made by Gemini about fixing
little issues I had (nothing really important, but why
not try to fix 'em?), then moving to optimization,
refactoring, and so on.

There's no reason to merge this repo as is, it's a mess. My only point is:
I use this tree for my job, every day. If there's something
good for the real developers, I would be happy to
contribute just by saying: it works for me!

If someone is going to cherry-picking, I am glad and happy.

## Short change history

It started with something like this:

```text
2026-10-05T15:36:22.679697+02:00 glet kernel: wmaker[9511]: segfault at 555002df5af9 ip 000055555559e384 sp 00007fffffffdae0 error 4 in wmaker[555555569000+6b000] likely on CPU 3 (core 3, socket 0)
2026-10-07T12:40:30.486599+02:00 glet kernel: wmaker[10212]: segfault at 8 ip 000055555559e39d sp 00007fffffffdae0 error 4 in wmaker[4a39d,555555569000+6b000] likely on CPU 7 (core 3, socket 0)
2026-10-13T16:33:37.234901+02:00 glet kernel: wmaker[1493526]: segfault at 968 ip 00007ffff7dccac0 sp 00007fffffffc030 error 4 in libX11.so.6.4.0[2aac0,7ffff7dbb000+90000] likely on CPU 4 (core 0, socket 0)
2026-10-14T16:54:17.831574+02:00 glet kernel: wmaker[9498]: segfault at 555000ba30b6 ip 00007ffff7f0d2da sp 00007fffffffbab0 error 4 in libWUtil.so.5.0.0[112da,7ffff7f01000+10000] likely on CPU 0 (core 0, socket 0)
2026-10-26T17:59:30.801140+01:00 glet kernel: wmaker[9514]: segfault at 968 ip 00007ffff7dc138d sp 00007fffffffcf90 error 4 in libX11.so.6.4.0[2038d,7ffff7dba000+91000] likely on CPU 1 (core 1, socket 0)
2026-10-27T16:26:06.981542+01:00 glet kernel: wmaker[11295]: segfault at 968 ip 00007ffff7dc138d sp 00007ffff359ee50 error 4 in libX11.so.6.4.0[2038d,7ffff7dba000+91000] likely on CPU 1 (core 1, socket 0)
2026-11-02T11:58:47.350676+01:00 glet kernel: wmaker[193746]: segfault at 51 ip 000055555559e903 sp 00007fffffffda50 error 4 in wmaker[4a903,55555556a000+6c000] likely on CPU 6 (core 2, socket 0)
```

Then I check out the official repo, and ask Gemini to watch out for something (if possible!).
It finds a Use-After-Free, so, let's have a second WMaker compiled (in `/opt/wmaker`) to test the fix (by the way, Ubuntu 24.04).

Sooo my new happy game begins: I keep using wmaker while agy (antigravity-cli) spends time looking at the code, patching it, recompiling it, and restarting it!

Next step, let's run the usual `cppcheck`, `clang`, `scan-build`, `flawfinder` and so on to have some sort of static code analysis.

Then: start using wmaker inside `valgrind`, to find usual malloc-related issues (memory leaks, and so on).

So, ask Gemini to implement support for KSM and Huge Pages.

It's good, so tell him to rewrite/refactor codebase based on the output of `perf` and `valgrind` (callgrind mode) attached to wmaker, to use modern interface to kernel/X11 facility, with the idea to rewrite algorithms to get the jobs done with much more less CPU cycles and interrupts. The results, just looking at the raw numbers, are wow! (Well, DeepSeek evaluation has some concerns!)

At the moment, it's keeping a little bit of telemetry in `/tmp/wmaker_runtime_logs/` to evaluate later, and adding some stress tests to speed up finding bugs related to recent changes.

Anyway, nights are going letting Gemini and DeepSeek arguing each other as a crazy wedding or, as they call it, doing *Reflection*.

Also, [Coverity Scan](https://scan.coverity.com/projects/gelma-wmaker) fixes are in, right now. I left all the analysis and related files in dir [CoverityReport](https://github.com/Gelma/wmaker/blob/master/CoverityReport), for a few reasons: easy way to access them for the [main project](http://windowmaker.org) - just in case - and track the approach differences between Gemini and DeepSeek to implement fixes.
Fun fact the relation chain of: open tools finding issues, Coverity finding much more, Gemini/DeepSeek extrapolate the false positive finding real root case, and while patching 'em, find two more issues not saw by Coverity!

You can find each step documented in [SpecAI.md](https://github.com/Gelma/wmaker/blob/master/SpecAI.md) and [git log](https://github.com/Gelma/wmaker/commits/master/), of course.

---
### (Original README)
---
# GNU Window Maker
### X11 Window Manager

<http://windowmaker.org>

#### by

- Alfredo K. Kojima
- Dan Pascu

#### Web/FTP Master

- Phillip Smith

> Congratulations! You have purchased an extremely fine
> device that would give you thousands of years of trouble-free
> service, except that you undoubtedly will destroy it via some
> typical bonehead consumer maneuver. Which is why we ask you
> to PLEASE FOR GOD'S SAKE READ THIS OWNER'S MANUAL CAREFULLY
> BEFORE YOU UNPACK THE DEVICE. YOU ALREADY UNPACKED IT, DIDN'T
> YOU? YOU UNPACKED IT AND PLUGGED IT IN AND TURNED IT ON AND
> FIDDLED WITH THE KNOBS, AND NOW YOUR CHILD, THE SAME CHILD WHO
> ONCE SHOVED A POLISH SAUSAGE INTO YOUR VIDEOCASSETTE RECORDER
> AND SET IT ON "FAST FORWARD", THIS CHILD ALSO IS FIDDLING
> WITH THE KNOBS, RIGHT? AND YOU'RE JUST NOW STARTING TO READ
> THE INSTRUCTIONS, RIGHT??? WE MIGHT AS WELL JUST BREAK THESE
> DEVICES RIGHT AT THE FACTORY BEFORE WE SHIP THEM OUT, YOU
> KNOW THAT?
>
> — Dave Barry, *"Read This First!"*

# Description

Window Maker is the GNU window manager for the X Window System. It was designed
to emulate the look and feel of part of the NEXTSTEP(tm) GUI. It's supposed to
be relatively fast and small, feature rich, easy to configure and easy to use,
with a simple and elegant appearance borrowed from NEXTSTEP(tm).

Window Maker was designed keeping integration with GNUstep in mind and is the
"official" window manager for it. It is also part of the GNU project
(www.gnu.org) Read more about GNUstep further on this file.

Hints (information given by applications to integrate well with the window
manager) for Motif(tm) and NETWM are also supported (NETWM is used by KDE and
GNOME, so they are automatically supported as a result). So you can replace any
of the window managers for these environments with Window Maker while keeping
most, if not all, of the native window manager functionality.

Window Maker was previously called WindowMaker.

Window Maker has no connection with Windowmaker, the software for making windows
and doors.

# Documentation

Read before asking.

* `README` files all over the source tree have info related to the stuff
  in the directories.

* `INSTALL` has installation instructions and some troubleshooting tips.
  You're meant to read it if you are compiling the sources.

* `FAQ`: Frequently Asked Questions. READ IT!!! `FAQ.I18N` is for
  internationalization related questions.

* `NEWS`: list user visible changes from the previous version. Read it if
  you are upgrading.

* `BUGFORM`: use it to send bug reports. Please do use it.

* `ChangeLog`: what changed from the previous version?

* `BUGS`: list of known bugs

### User Guide

The Window Maker User's Guide can be downloaded from the official ftp
or web sites.

The User Guide explains how to use Window Maker, the configuration files
and options.

### man pages

Type `man wmaker` in the shell prompt to get general help about Window Maker

# Directories & Files in the Source Tree

* `Install` is a script for configuring and compiling Window Maker in a easy
  way (not that the normal way is difficult, but...).

* `AUTHORS`: the credits

* `TODO`: plans for the future

* `util/` has various utility programs.

* `WPrefs.app/` is the configuration program.

* `src/wconfig.h` has compile time options you can change to select some
  options/features and other stuff.

* `WINGs/` NEXTSTEP lookalike widget library

* `wrlib/` image processing library

* `po/` has message catalogs which are the translated versions of the messages
  displayed by Window Maker.

# HELP ME!!!

There is a mailing list for discussing Window Maker at
<wmaker-user@googlegroups.com>. To subscribe to it, visit
https://groups.google.com/forum/#!forum/wmaker-user

If you got a problem, ask there (after reading the docs, of course). The people
there is more likely to be able to answer your questions than we are. For bug
reports use the `BUGFORM`.

If you have a problem with a precompiled version of Window Maker (rpm, deb etc),
first ask the person who made the package.

**IMPORTANT NOTE**: when asking for help (in the mailing list or to the developers,
directly) *always* send information about the system you are using. You can use
the system information section at the end of `BUGFORM` as a guideline. Another
thing: please don't send HTML mail.

There is also a `#windowmaker` IRC channel at freenode. To join there, connect
your irc client to `irc.freenode.net`. `irc.windowmaker.org` will direct you to the
correct IRC network.

# GNUstep

GNUstep is a complete object-oriented development system, based on the OpenStep
specification released by NeXT(tm) (now Apple(tm)) and Sun(tm). It will provide
everything one needs to produce cross-platform, object-oriented, graphical (and
non-graphical) applications; providing among other things, base system
libraries, a high-level GUI application framework that uses a Display
PostScript(tm)-like imaging model (DGS), objects for accessing relational
databases, distributed objects and a graphical development environment, with
tools like interface modeler, a project management system (project center) and
other tools.

The GNUstep development system will be used to create a user environment, with
everything needed for a complete graphical user interface, such as a file
viewer, text editors and other applications. Note that the user environment (or
"desktop environment") is only a small part of the whole GNUstep project and
therefore it does not "compete" with other projects like KDE or GNOME, simply
because they are completely different things.

For more information on the GNUstep project, visit: <http://www.gnustep.org>

# Running multiple instances of Window Maker

It is not a good idea to run more than one instance of Window Maker
from the same user (so that wmaker will use the same configuration files) at the
same time. You might get unexpected behavior when Window Maker updates its
configuration files.

If you really desire to do this, try running Window Maker with the command line
option `--static` so that it will not update or change any of the configuration
files.

# Performance Tuning

If you want to diminish Window Maker's memory usage and improve performance,
while keeping a nice appearance and good functionality, follow the items below:

- use solid textures for everything, mainly title bars and menus. If you want a
  nice looking desktop, use the Traditional style.
- turn `NewStyle` and `Superfluous` off
- do not bind many shortcuts in the menu and keep only the essential items in
  the menu
- turn on `DisableClip`
- edit `wconfig.h` and disable the `NUMLOCK_HACK` and the features you don't use
  anyway (keep in mind that some of the `#defines` might not work, as they are not
  fully supported). Make sure to always keep NumLock and ScrollLock turned off.
- turn on `DisableAnimations`. You can also specify `--disable-animation` at compile
  time to the configure script.
- strip down the default `IconPath` and `PixmapPath` entries to contain only
  the paths that you really have in your system.
- do not use large images in the root background
- remove support for image formats you don't use
- to reduce memory usage, disable the icon cache, by setting the `RIMAGE_CACHE`
  environment variable to `0`. If you want to increase performance at the cost of
  memory usage, set its value to a value like the number of different icons you
  use. Also, disable anti-aliased text support in `~/GNUstep/Defaults/WMGLOBAL`.

# Keyboard Mouse Control

Many people ask about adding keyboard control of mouse, like in fvwm, but Window
Maker will not have such feature. The XKB extension supports mouse simulation
from the keyboard, in a much more powerful fashion than any simulation done by
a window manager.

To enable it, hit the Control+Shift+NumLock or Shift+NumLock key combination.
You should hear the speaker beeping. To disable it, do the same thing.

To control the mouse the numeric keypad is used as follows:
- `4` (left arrow), `7` (Home), `8` (up arrow), `9` (PgUp), `6` (right arrow),
  `3` (PgDn), `2` (down arrow) and `1` (End) move the mouse to the corresponding
  direction;
- holding one of the above keys and then holding the `5` key will move the
  pointer faster;
- `/` will select the first mouse button (left button);
- `*` will select the second mouse button (middle button);
- `-` will select the third mouse button (right button);
- `5` will do a click with the currently selected mouse button;
- `+` will do a double-click with the currently selected button;
- `0` (Ins) will click and hold the currently selected button;
- `.` (Del) will release the currently selected button that was previously
  clicked with the `0` (Ins) key.

The above key values work on an XFree86 3.2 (X11R6.1) X server, but your
mileage may vary.

# How to make a gdb backtrace

Backtraces can help us fix bugs that make Window Maker crash. If you find a bug
that crashes Window Maker, please send a backtrace with your bug report.

To make a useful backtrace, you need a core file with debugging information
produced by Window Maker when it crashes. It should have been installed without
being stripped.

To compile wmaker with debugging information:

```bash
./configure --enable-debug
make
```

If you get the dialog window telling you that wmaker crashed and asks you what
to do, tell it to "Abort and leave a core file".

```bash
script
cd src
gdb .libs/wmaker path_to_the_core_file
```

Then, in the gdb prompt type `bt`. Quit from gdb by typing `quit` and then, in
the shell prompt, type `exit`.

The file named `typescript` will contain the backtrace.

# Copyrights & Disclaimers

Window Maker is copyrighted by Alfredo K. Kojima and is licensed through the GNU
General Public License. Read the `COPYING` file for the complete license.

The icons that are distributed with this program and were made by Marco van
Hylckama Vlieg, are licensed through the GNU General Public License. Read the
`COPYING` file for the complete license.

The icons listed in `COPYING.WTFPL` and are distributed in this program and were
made by Banlu Kemiyatorn (]d), are licensed through the "do What The Fuck you
want to Public License". Read the `COPYING.WTFPL` file for the complete license.

NeXT, OpenStep and NEXTSTEP are trademarks of NeXT Computer, Inc. All other
trademarks are property of their respective owners.

The authors reserve the right to make changes in the software without prior
notice.

# Authors

- Alfredo K. Kojima <kojima@windowmaker.info>
- Dan Pascu <dan@windowmaker.info>
- ]d <id@windowmaker.info>

Please don't ask us questions before reading the documentation (esp. the FAQ,
this file and INSTALL files) and about "cool" things you see in people's desktop
screenshots.

The `AUTHORS` file contains a list of the people who have contributed to the
project. The names of people who have helped with localization (translation) can
be found in `po/README` and `Window Maker/README`.

If you have any comments, fixes and bug reports (filled BUGFORMs) send them
to <wmaker-dev@googlegroups.com>.
