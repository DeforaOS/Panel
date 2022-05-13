DeforaOS Panel
==============

About Panel
-----------

This program is meant to display an arbitrary number of desktop panels, with
support for up to one per edge of the screen at the moment. Each panel can then
host a number of applets, implemented as plug-ins to the main program.

A number of additional utilities is also available, allowing tighter integration
of external applications, as well as when responding to system events
(notifications...).

Compiling Panel
---------------

Panel depends on the following components:

 * Gtk+ 2.4 or newer, or Gtk+ 3.0 or newer
 * DeforaOS libDesktop
 * DeforaOS Browser
 * an implementation of `make`
 * gettext (libintl) for translations
 * docbook-xsl for the documentation (optional)

With these installed, the following command should be enough to compile and
install Panel on most systems:

    $ make install

To install (or package) Panel in a different location, use the `PREFIX` option
as follows:

    $ make PREFIX="/another/prefix" install

Panel also supports `DESTDIR`, to be installed in a staging directory; for
instance:

    $ make DESTDIR="/staging/directory" PREFIX="/another/prefix" install

The compilation process supports a number of other options, such as OBJDIR for
compilation outside of the source tree for instance.

On some systems, the Makefiles shipped may have to be re-generated accordingly.
This can be performed with the DeforaOS configure tool.

Documentation
-------------

Manual pages for each of the executables installed are available in the `doc`
folder. They are written in the DocBook-XML format, and need libxslt and
DocBook-XSL to be installed for conversion to the HTML or man file format.

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
