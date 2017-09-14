DeforaOS Panel
==============

About Panel
-----------

This program is meant to display an arbitrary number of desktop panels, with
support for one per edge of the screen at the moment. Each panel can then host a
number of applets, implemented as plug-ins to the main program.

A number of additional utilities is also available, allowing tighter integration
of external applications, as well as when responding to system events
(notifications...).

Documentation
-------------

Each of these tools, as well as Panel itself, are documented in a series of
manual pages. They are also available in the HTML format.

Compiling Panel
---------------

The current requirements for compiling Panel are as follows:
 * Gtk+ 2.4 or later, or Gtk+ 3.0 or later
 * DeforaOS libDesktop
 * DeforaOS Browser
 * an implementation of `make`
 * gettext (libintl) for translations
 * docbook-xsl for the documentation (optional)

With these installed, the following command should be enough to compile Panel on
most systems:

    $ make

On some systems, the Makefiles shipped can be re-generated accordingly thanks to
the DeforaOS configure tool.

The compilation process supports a number of options, such as PREFIX and DESTDIR
for packaging and portability, or OBJDIR for compilation outside of the source
tree.

Extending Panel
---------------

Applets for the Panel program can be written according to the API definitions
installed and found in <Desktop/Panel.h> (here in `include/Panel.h`).

A sample applet can be found in `src/applets/template.c`.

Further applets can also be found in the DeforaOS Integration project.

Distributing Panel
------------------

DeforaOS Panel is subject to the terms of the GPL license, version 3. Please see
the `COPYING` file for more information.
