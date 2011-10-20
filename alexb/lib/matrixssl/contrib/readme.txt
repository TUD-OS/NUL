MatrixSSL Directory Structure

matrixssl/
	this directory contains files the implement the SSL and TLS protocol.
	test/
		single-process SSL handshake test application

core/
	pool based malloc() implementation*
	utility functions
	POSIX/
		Operating system layer for Linux, BSD
		TCP layer for Linux, BSD and Windows
	WIN32/
		Operating system layer for Windows NT, 2K, XP, Vista, 7

crypto/
	digest/
		message digests: md5, sha-1, sha-256*, hmac, etc.
	keyformat/
		key parsing routines for x.509, base64 and asn.1 data formats
	math/
		large integer math operations
	prng/
		psuedo random number generation
	pubkey/
		RSA and DH* operations
		PKCS enccoding and decoding of keys
	sampleCerts/
		example certificates in various formats
	symmetric/
		symmetric ciphers: arc4, 3des, aes, seed*

apps/
	example SSL client using blocking sockets and session resumption
	example SSL server using non-blocking sockets and simultaneous connections

doc/
	release notes
	developer guides
	api documentation

sampleCerts/
	sample RSA keys and certificate files for testing and example apps

* utilities/ 
	certgen - generate X.509 cert from a certificate request or self-signed
	certrequest - generate a cert request from a private RSA key
	pemtomem - convert a pem format key or certificate to C header
	rsakeygen - generate an RSA public/private keypair


* Asterisk denotes commercial licensed version only
