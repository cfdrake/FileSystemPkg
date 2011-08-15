EDK II FileSystemPkg
====================

Introduction
------------
This module provides a driver that exposes EFI_FIRMWARE_VOLUME2_PROTOCOL
instances as read-only EFI_SIMPLE_FILE_SYSTEM_PROTOCOL-compatible file
systems. This enables users and programs to explore and read data from an FV2
instance from a simple file-like perspective.

Design
------
TODO.

Bugs
----
I believe I've fixed everything I've come across so far. If you see anything,
please send a message to colin.f.drake@gmail.com.

Authors/License
---------------
See each individual file for authorship and licensing. In short, everything is
BSD-licensed.
