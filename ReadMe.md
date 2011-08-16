<link href="http://kevinburke.bitbucket.org/markdowncss/markdown.css" rel="stylesheet"></link>
EDK II FileSystemPkg
====================

Introduction
------------
This module provides a driver that exposes `EFI_FIRMWARE_VOLUME2_PROTOCOL`
instances as read-only `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL`-compatible file
systems. This enables users and programs to explore and read data from an FV2
instance from a simple file-like perspective.

Installation/Integration
------------------------
First, download a recent `EDKII` release. `cd` to the root directory and execute
the following command to obtain the source for `FileSystemPkg`:

    $ git clone git@github.com:cfdrake/FileSystemPkg.git

Next, add a reference to the FileSystemPkg module GUID, specified in `Ffs.inf`, and
a reference to `Ffs.inf` itself in the `.dec` and `.dsc` files of your build environment
of choice.

Finally, build the project. The driver should be integrated and you should be able to
mount FV2 instances from the shell.

Design
------
TODO.

Bugs
----
I believe I've fixed everything I've come across so far. If you see anything,
please send a message to colin.f.drake@gmail.com.

I'm sure there's something ;)

Authors/License
---------------
See each individual file for authorship and licensing. In short, everything is
BSD-licensed.
