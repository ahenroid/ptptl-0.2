The Peer-to-Peer Trusted Library (Release 0.2)

Copyright (c) 2001 Intel Corporation. All rights reserved.

MOTIVATION
==========

Peer-to-peer applications are becoming increasingly
ubiquitous and security is a vital concern for any modern
peer-to-peer software.  With this project, we hope to
spur open innovation in the peer-to-peer security space
and provide a basis for an open-source development project
focussing on the security aspects of peer-to-peer.


OVERVIEW
========

The Peer-to-Peer Trusted Library (PtPTL) allows software
developers to add the element of ``Trust'' to their
peer-to-peer applications.  It provides support for
digital certificates, peer authentication, secure storage,
public key encryption, digital signatures, digital
envelopes, and symmetric key encryption.  The library also
provides simple support for networking and some operating
system primitives, such as threads and locks, to ease the
development of applications that are portable to both Win32
and Linux.

This release includes full API documentation and several
sample applications .  The samples can be built and used,
as is, for tasks such as simple certificate management and
secure file sharing or they can be enhanced or integrated
into existing applications.


OPENSSL
=======

The PtPTL is built upon the open-source OpenSSL Toolkit.
OpenSSL provides all low-level certificate and cryptographic
support and the PtPTL provides high-level and easy-to-use
interfaces to OpenSSL.  For more information on the OpenSSL
project see ``http://www.openssl.org''

Both the PtPTL and OpenSSL are licensed under BSD or BSD-like
licenses and have minimal restrictions for the use of the code
in open-source or commercial products.  Please note, though,
that the OpenSSL license is independent of the PtPTL license.
See the file ``openssl/LICENSE'' for the OpenSSL licensing
terms.

Pre-built binaries of the OpenSSL cryptographic library are
no longer provided with the distribution, but are created during
the normal build process.  OpenSSL is preconfigured for Win32
and Linux (ELF, debug).  If any other configurations are
necessary, you must reconfigure and build the OpenSSL library.
See ``openssl/INSTALL'' and ``openssl/INSTALL.W32'' for more
information.


DISCLAIMER
==========

Like any other software, there can never be a complete assurance
that the PtPTL, the source code and cryptographic algorithms it
contains, or the OpenSSL cryptographic library are 100% secure.
See the ``LICENSE.txt'' file for the complete disclaimer.


DOCUMENTATION
=============

HTML API documentation for the PtPTL are provided in the
``docs'' directory.  The PtPTL has been built with a unique
internal documentation system, similar to that used by the
Gnome Project (http://www.gnome.org) and the Linux kernel.
Basically, a tool is used to extract API information
from the source code and build HTML documentation. See
``docs/mkdoc.pl'' for more information.


QUESTIONS
=========

What follows is a simple, but growing, list of questions and
answers about the PtPTL and peer-to-peer security.

Q. Why might a developer base their application on the PtPTL
   instead of just using SSL?

A. SSL works well for a client-server network model, but
   peer-to-peer has different requirements.  Many peer-to-peer
   applications distribute data among multiple clients and this
   network model is ideally not implemented as a series of
   client-server connections.  The PtPTL provides the flexibility
   to support a wide variety of peer-to-peer network models.

   Also, the PtPTL allows new methods of peer authentication,
   such as authentication hardware and biometric devices, to
   be cleanly integrated into the PtPTL architecture.

Q. Why use the PtPTL instead of another existing security architecture?

A. The PtPTL is portable to a wide variety of platforms including
   Win32 and Linux, with no licensing cost and minimal licensing
   restrictions.  The PtPTL utilizes the portability of the OpenSSL
   cryptography library to be itself portable.

   Also, the PtPTL is open-source software.  Source code access
   eases development and test efforts and ensures stability,
   which is vitally important for secure applications.  Open-source
   also gives developers a chance to participate in the design
   and future direction of the project.

Q. Is the PtPTL based on any cryptographic and/or network standards?

A. Yes, the PtPTL conforms to existing standards where they exist,
   and includes support for
   * X.509 digital signatures
   * PKCS#1 (RSA cryptography)
   * PKCS#5 (password-based cryptography)
   * PKCS#7 (digital envelopes)
   * PKCS#12 (personal information exchange)
   * RFC 1421 (privacy enhanced mail format)
   * Various standard symmetric encryption algorithms
   * HTTP


WIN32 INSTALLATION
==================

You will need the Visual C++ compiler and build environment
installed to build the library and sample applications.

All of the components of the library can be built in one
pass using nmake.  To build with nmake, from a command prompt
within the VC environment, after running vcvars32.bat or the
equivalent, enter

  nmake -f Makefile.mak

After a successful build, you can install the sample executables
into the "bin" directory with

  nmake -f Makefile.mak install

By default, the library is built as debug.  If you wish to produce
a release build, edit the ``Makefile.cfg'' file and rebuild. Make
sure that you do a

  nmake -f Makefile.mak clean

before rebuilding, to ensure that old object files and libraries
are removed.


LINUX INSTALLATION
==================

You will need the GNU g++ compiler and GNU make installed to
build the library and sample applications.

From the command line, simply type

  make

The library is built as debug by default.  If you wish to
produce a non-debug build, edit the ``Makefile.cfg'' file
and rebuild.  Make sure that you do a

  make clean

before rebuilding, to ensure that old object files and libraries
are removed.


BUG REPORTS
===========

Please send bug reports to ``henroid@users.sourceforge.net''
with as much information about the bug, how to reproduce it,
your platform, and the version of the PtPTL as possible.


CONTRIBUTING
============

Contribution to the PtPTL project is highly encouraged.
Get more details about the project status and how you can
help from the SourceForge PtPTL project page
``http://sourceforge.net/projects/ptptl''.
