Onload Test Suite library
=========================

This repository contains header files and source code of libraries that are common to various Onload Test Suites.
This is currently applied in Zetaferno TS and Socket tester TS.

Building
--------

Building libraries is the task of a particular Test Suite.
In the current version, the required libraries are used to build the main library in TS.
For example, in Socket Tester TS the name of this library will be libts_sockapi.

In the future, it is expected to build through the Meson Build system.

License
-------

See license file in the repository.
