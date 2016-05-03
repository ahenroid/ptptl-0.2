Cert
====

Cert is a simple digital certificate manager.  It can be used to
create, import, export, and store digital certificates in PKCS#12
format.  Cert also creates new (randomly-generated) RSA public/private
key pairs from which it creates new certificates.

On Win32, certificates and keys are stored in the registry in
`HKEY_CURRENT_USER/Software/PTL/Cert'.  On Linux, they are currently
stored in `$HOME/.ptl/cert'.

Setup
=====

The first step in using Cert is to setup a local digital certificate
and RSA key.  This can be done from the command line like this

  cert --setup "John Doe"

Running
=======

Once your local certificate and key are setup, you can
* Export a certificate:  cert --export "Jane Doe" janedoe.pfx
* Import a certificate:  cert --import janedoe.pfx
* Sign a certificate:    cert --sign janedoe.pfx 30
* Remove a certificate:  cert --remove "Jane Doe"
* Display a certificate: cert janedoe.pfx
* List all certificates: cert

Advanced setup
==============

To create a set of signed certificates, use the setup.pl script.
It creates a set of certificates, signs them with a created
certificate authority, and creates a certificate store for each
(naming them user0.pfx, user1.pfx, user2.pfx, ...)

For example, the following command creates two certificates
("John Doe" and "Jane Doe") and signs them with a third
certificate ("Root CA") for a period of 45 days.

  setup.pl --issuer "Root CA" --expire 45 "John Doe" "Jane Doe"

Now, the certificate store "user0.pfx" contains a signed
certificate for "John Doe", a private key for "John Doe",
and signed certificates for "Jane Doe" and "Root CA".

The certificate store "user1.pfx" contains a signed
certificate for "Jane Doe", a private key for "Jane Doe",
and signed certificates for "John Doe" and "Root CA".

On John Doe's system, use the following command to install
his certificate store

  cert --load user0.pfx

