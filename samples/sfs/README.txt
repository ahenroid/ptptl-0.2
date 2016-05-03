Secure File Sharing Application
===============================

SFS is yet another simple secure file sharing application.  It
exchanges digital certificates, authenticates peers, searches,
and transfers files all over HTTP.  SFS is considerably more
versatile than the Trutella application, especially with
regard to certificate distribution.

Setup
=====

Digital certificates for all of the clients should be setup
before using SFS.  See the sample certificate manager "cert"
and its documentation for setup details.  As long as all
certificates are signed by a trusted certificate authority,
certificates will be exchanged automatically during the
authentication process.

Running
=======

Searching
* Type "search HOST PATTERN" where HOST is the URL you wish
  to search and PATTERN is the search string.  For example,

    search testserver.test.com:8080 *.jpg

* SFS will print a list of search responses as they arrive.

Downloading a file
* Type "get ID PATH" where ID is the search response number
  and PATH is the pathname to store the file.

Sharing files
* Type "share DIR" where DIR is the directory to share.
  When all directories are shared, type "wait" to handle
  client connections.


