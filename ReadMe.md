EDK II FileSystemPkg
====================

Introduction
------------
This module provides a driver that exposes `EFI_FIRMWARE_VOLUME2_PROTOCOL`
instances as read-only `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL`-compatible file
systems. This enables users and programs to explore and read data from an `FV2`
instance from a simple file-like perspective.

It was developed as a Google Summer of Code project by
[Colin Drake](http://colinfdrake.com) over the summer of 2011.

Source
------
This module is currently maintained at http://github.com/cfdrake/FileSystemPkg.

Checkout the `development` branch for the latest changes or `master` for a (hopefully)
more stable revision. Tags are made for each major revision.

Installation/Integration
------------------------
First, download a recent EDKII release. `cd` to the root directory and download the
`FileSystemPkg` sources (either `master` or `development` branch).

    $ ...
    $ cd edk2
    $ git clone git@github.com:cfdrake/FileSystemPkg.git

Next, add a reference to the FileSystemPkg module GUID, specified in `Ffs.inf`, and
a reference to `Ffs.inf` itself in the `.dec`, `.fdf`, and `.dsc` files of your build
environment of choice.

Finally, build the project. The driver should be integrated and you should be able to
mount `FV2` instances from the shell.

    $ cd <YourDevEnvironment>
    $ ./build.sh
    ...
    $ ./build.sh run

Bugs
----
I think I've fixed everything I've come across so far. If you see anything 
though, please send a message to colin.f.drake@gmail.com or create a new issue at
the [bug tracker](https://github.com/cfdrake/FileSystemPkg/issues).

I'm sure there's something ;)

Authors/License
---------------
See each individual file for authorship and licensing. In short, everything is
BSD-licensed.
