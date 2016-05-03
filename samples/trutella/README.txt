Trutella
========

Trutella is a simple secure file sharing application.  It uses the
Gnutella network protocol and is a complete Gnutella client.  Trutella
also supports the notion of secure groups, private file sharing groups
for which members can choose who to let in and who to keep out.
Secure group requests are encapsulated inside Gnutella requests, so
they are passed freely about the GnutellaNet.

Setup
=====

Digital certificates for the local user and all participating secure
group users must be setup before using Trutella.  See the sample
certificate manager "cert" and its documentation for setup details.

Running (Win32 GUI)
===================

Connecting to another client
* From the "File" menu, select "Preferences"
* Goto the "Connections" tab
* Type in the host (eg. gnut, gnut:6346, 10.0.0.1:6346) and click "Add"
* Click "OK"

Creating/joining a secure group
* From the "File" menu, select "Preferences"
* Goto the "Groups" tab
* Type in a new group name and click "Join/Create"
* Click "OK"

Sharing a directory
* From the "File" menu, select "Preferences"
* Goto the "Sharing" tab
* Click "Add" and select a directory
* Set the "Share with" secure group or "All of GnutellaNet"
* Set file extensions to match
* Click "OK"

Searching
* Return to the main application window
* Enter search text in the "Search for" box
* Select the search group
* Click "Search"
* Trutella will display search responses as they arrive

Downloading a file
* Return to the main application window
* Simply double-click on any search result to download

Running (Console mode)
======================

Connecting to another client
* Type "open HOST" where HOST is the destination host

Creating/joining a secure group
* Type "join GROUP" where GROUP is the group name

Sharing a directory
* Type "share DIR" where DIR is the directory.  The
  directory will be shared with your current secure
  group (last "join").

Searching
* Type "search STRING" where STRING is the search string.
* Trutella will print a list of search responses as
  they arrive.

Downloading a file
* Type "get NUMBER" where NUMBER is the index of
  a search result.



