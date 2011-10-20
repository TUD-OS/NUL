/*
 *	client.c
 *  Release $Name: MATRIXSSL-3-2-2-OPEN $
 *
 *	Simple MatrixSSL blocking client example
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2011. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#include <time.h>
#include "app.h"
#include "matrixssl/matrixsslApi.h"

#ifdef USE_CLIENT_SIDE_SSL

#ifdef WIN32
#pragma message("DO NOT USE THESE DEFAULT KEYS IN PRODUCTION ENVIRONMENTS.")
#else
#warning "DO NOT USE THESE DEFAULT KEYS IN PRODUCTION ENVIRONMENTS."
#endif

#define USE_HEADER_KEYS
#define ALLOW_ANON_CONNECTIONS	1

#ifdef USE_HEADER_KEYS
#include "sampleCerts/CAcertSrv.h"
#else
static char CAcertSrvFile[] = "CAcertSrv.pem";
#endif /* USE_HEADER_KEYS */


/* #define REHANDSHAKE_TEST */

/********************************** Globals ***********************************/

static unsigned char g_httpRequestHdr[] = "GET / HTTP/1.0\r\n"
	"User-Agent: MatrixSSL/" MATRIXSSL_VERSION "\r\n"
	"Accept: */*\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

/********************************** Defines ***********************************/

//#define HTTPS_IP				(char *)"127.0.0.1"
#define HTTPS_IP				(char *)"141.76.49.53"

/****************************** Local Functions *******************************/

static int32 httpWriteRequest(ssl_t *ssl);
static int32 certCb(ssl_t *ssl, psX509Cert_t *cert, int32 alert);
static SOCKET socketConnect(char *ip, int32 port, int32 *err);
static void closeConn(ssl_t *ssl, SOCKET fd);


/******************************************************************************/
/*
	Make a secure HTTP request to a defined IP and port
	Connection is made in blocking socket mode
	The connection is considered successful if the SSL/TLS session is
	negotiated successfully, a request is sent, and a HTTP response is received.
 */
static int32 httpsClientConnection(sslKeys_t *keys, sslSessionId_t *sid)
{
	int32			rc, transferred, len, complete;
	ssl_t			*ssl;
	unsigned char	*buf;
	httpConn_t		cp;
	SOCKET			fd;
	
	complete = 0;
	memset(&cp, 0x0, sizeof(httpConn_t));
	fd = socketConnect(HTTPS_IP, 9999, &rc); //HTTPS_PORT, &rc);
	if (fd == INVALID_SOCKET || rc != PS_SUCCESS) {
		_psTraceInt("Connect failed: %d.  Exiting\n", rc);
		return PS_PLATFORM_FAIL;
	}
	
	rc = matrixSslNewClientSession(&ssl, keys, sid, 0, certCb, NULL, NULL);
	if (rc != MATRIXSSL_REQUEST_SEND) {
		_psTraceInt("New Client Session Failed: %d.  Exiting\n", rc);
		close(fd);
		return PS_ARG_FAIL;
	}
WRITE_MORE:
	while ((len = matrixSslGetOutdata(ssl, &buf)) > 0) {
		transferred = send(fd, buf, len, 0);
		if (transferred <= 0) {
			goto L_CLOSE_ERR;
		} else {
			/* Indicate that we've written > 0 bytes of data */
			if ((rc = matrixSslSentData(ssl, transferred)) < 0) {
				goto L_CLOSE_ERR;
			}
			if (rc == MATRIXSSL_REQUEST_CLOSE) {
				closeConn(ssl, fd);
				return MATRIXSSL_SUCCESS;
			} 
			if (rc == MATRIXSSL_HANDSHAKE_COMPLETE) {
				/* If we sent the Finished SSL message, initiate the HTTP req */
				/* (This occurs on a resumption handshake) */
				if (httpWriteRequest(ssl) < 0) {
					goto L_CLOSE_ERR;
				}
				goto WRITE_MORE;
			}
			/* SSL_REQUEST_SEND is handled by loop logic */
		}
	}
READ_MORE:
	if ((len = matrixSslGetReadbuf(ssl, &buf)) <= 0) {
		goto L_CLOSE_ERR;
	}
	if ((transferred = recv(fd, buf, len, 0)) < 0) {
		goto L_CLOSE_ERR;
	}
	/*	If EOF, remote socket closed. But we haven't received the HTTP response 
		so we consider it an error in the case of an HTTP client */
	if (transferred == 0) {
		goto L_CLOSE_ERR;
	}
	if ((rc = matrixSslReceivedData(ssl, (int32)transferred, &buf,
									(uint32*)&len)) < 0) {
		goto L_CLOSE_ERR;
	}
	
PROCESS_MORE:
	switch (rc) {
		case MATRIXSSL_HANDSHAKE_COMPLETE:
#ifdef REHANDSHAKE_TEST
/*
			Test rehandshake capabilities of server.  If a successful
			session resmption rehandshake occurs, this client will be last to
			send handshake data and MATRIXSSL_HANDSHAKE_COMPLETE will hit on
			the WRITE_MORE handler and httpWriteRequest will occur there.
			
			NOTE: If the server doesn't support session resumption it is
			possible to fall into an endless rehandshake loop
*/
			if (matrixSslEncodeRehandshake(ssl, NULL, NULL, 0, 0) < 0) {
				goto L_CLOSE_ERR;
			}
#else		
			/* We got the Finished SSL message, initiate the HTTP req */
			if (httpWriteRequest(ssl) < 0) {
				goto L_CLOSE_ERR;
			}
#endif
			goto WRITE_MORE;
		case MATRIXSSL_APP_DATA:
			if ((rc = httpBasicParse(&cp, buf, len)) < 0) {
				closeConn(ssl, fd);
				if (cp.parsebuf) free(cp.parsebuf); cp.parsebuf = NULL;
				cp.parsebuflen = 0;
				return MATRIXSSL_ERROR;
			}
			if (rc == HTTPS_COMPLETE) {
				rc = matrixSslProcessedData(ssl, &buf, (uint32*)&len);
				closeConn(ssl, fd);
				if (cp.parsebuf) free(cp.parsebuf); cp.parsebuf = NULL;
				cp.parsebuflen = 0;
				if (rc < 0) {
					return MATRIXSSL_ERROR;
				} else {
					if (rc > 0) {
						_psTrace("HTTP data parsing not supported, ignoring.\n");
					}
					_psTrace("SUCCESS: Received HTTP Response\n");
					return MATRIXSSL_SUCCESS;
				}
			}
			/* We processed a partial HTTP message */
			if ((rc = matrixSslProcessedData(ssl, &buf, (uint32*)&len)) == 0) {
				goto READ_MORE;
			}
			goto PROCESS_MORE;
		case MATRIXSSL_REQUEST_SEND:
			goto WRITE_MORE;
		case MATRIXSSL_REQUEST_RECV:
			goto READ_MORE;
		case MATRIXSSL_RECEIVED_ALERT:
			/* The first byte of the buffer is the level */
			/* The second byte is the description */
			if (*buf == SSL_ALERT_LEVEL_FATAL) {
				psTraceIntInfo("Fatal alert: %d, closing connection.\n", 
							*(buf + 1));
				goto L_CLOSE_ERR;
			}
			/* Closure alert is normal (and best) way to close */
			if (*(buf + 1) == SSL_ALERT_CLOSE_NOTIFY) {
				closeConn(ssl, fd);
				if (cp.parsebuf) free(cp.parsebuf); cp.parsebuf = NULL;
				cp.parsebuflen = 0;
				return MATRIXSSL_SUCCESS;
			}
			psTraceIntInfo("Warning alert: %d\n", *(buf + 1));
			if ((rc = matrixSslProcessedData(ssl, &buf, (uint32*)&len)) == 0) {
				/* No more data in buffer. Might as well read for more. */
				goto READ_MORE;
			}
			goto PROCESS_MORE;
		default:
			/* If rc <= 0 we fall here */
			goto L_CLOSE_ERR;
	}
	
L_CLOSE_ERR:
	_psTrace("FAIL: No HTTP Response\n");
	matrixSslDeleteSession(ssl);
	close(fd);
	if (cp.parsebuf) free(cp.parsebuf); cp.parsebuf = NULL;
	cp.parsebuflen = 0;
	return MATRIXSSL_ERROR;
}

/******************************************************************************/
/*
	Create an HTTP request and encode it to the SSL buffer
 */
static int32 httpWriteRequest(ssl_t *ssl)
{
	unsigned char   *buf;
	uint32          available, requested;

	requested = strlen((char *)g_httpRequestHdr) + 1;
	if ((available = matrixSslGetWritebuf(ssl, &buf, requested)) < 0) {
		return PS_MEM_FAIL;
	}
	requested = min(requested, available);
	strncpy((char *)buf, (char *)g_httpRequestHdr, requested);
	_psTraceStr("SEND: [%s]\n", (char*)buf);
	if (matrixSslEncodeWritebuf(ssl, strlen((char *)buf)) < 0) {
		return PS_MEM_FAIL;
	}
	return MATRIXSSL_REQUEST_SEND;
}

/******************************************************************************/
/*
	Main routine. Initialize SSL keys and structures, and make two SSL 
	connections, the first with a blank session Id, and the second with
	a session ID populated during the first connection to do a much faster
	session resumption connection the second time.
 */
int32 main(int32 argc, char **argv)
{
	int32			rc;
	sslKeys_t		*keys;
	sslSessionId_t	sid;
#ifdef WIN32
	WSADATA			wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif
	if ((rc = matrixSslOpen()) < 0) {
		_psTrace("MatrixSSL library init failure.  Exiting\n");
		return rc; 
	}
	if (matrixSslNewKeys(&keys) < 0) {
		_psTrace("MatrixSSL library key init failure.  Exiting\n");
		return -1;
	}

#ifdef USE_HEADER_KEYS
/*
	In-memory based keys
*/
	if ((rc = matrixSslLoadRsaKeysMem(keys, NULL, 0, NULL, 0, CAcertSrvBuf,
			sizeof(CAcertSrvBuf))) < 0) {
		_psTrace("No certificate material loaded.  Exiting\n");
		matrixSslDeleteKeys(keys);
		matrixSslClose();
		return rc;
	}
#else /* USE_HEADER_KEYS */
/*
	File based keys
*/
	if ((rc = matrixSslLoadRsaKeys(keys, NULL, NULL, NULL, CAcertSrvFile)) < 0){
		_psTrace("No certificate material loaded.  Exiting\n");
		matrixSslDeleteKeys(keys);
		matrixSslClose();
		return rc;
	}
#endif /* USE_HEADER_KEYS */


	matrixSslInitSessionId(sid);
	_psTrace("=== INITIAL CLIENT SESSION ===\n");
	httpsClientConnection(keys, &sid);

	_psTrace("\n=== CLIENT SESSION WITH CACHED SESSION ID ===\n");
	httpsClientConnection(keys, &sid);
	
	matrixSslDeleteKeys(keys);
	matrixSslClose();

#ifdef WIN32
	_psTrace("Press any key to close");
	getchar();
#endif
	return 0;
}

/******************************************************************************/
/*
	Close a socket and free associated SSL context and buffers
	An attempt is made to send a closure alert
 */
static void closeConn(ssl_t *ssl, SOCKET fd)
{
	unsigned char	*buf;
	int32			len;
	
	/* Set the socket to non-blocking to flush remaining data */
#ifdef POSIX
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#elif WIN32
	len = 1;		/* 1 for non-block, 0 for block */
    ioctlsocket(fd, FIONBIO, &len);
#endif
	/* Quick attempt to send a closure alert, don't worry about failure */
	if (matrixSslEncodeClosureAlert(ssl) >= 0) {
		if ((len = matrixSslGetOutdata(ssl, &buf)) > 0) {
			if ((len = send(fd, buf, len, MSG_DONTWAIT)) > 0) {
				matrixSslSentData(ssl, len);
			}
		}
	}
	matrixSslDeleteSession(ssl);
	if (fd != INVALID_SOCKET) close(fd);
}

/******************************************************************************/
/*
	Example callback to do additional certificate validation.
	If this callback is not registered in matrixSslNewService,
	the connection will be accepted or closed based on the status flag.
 */
static int32 certCb(ssl_t *ssl, psX509Cert_t *cert, int32 alert)
{
#ifdef POSIX
	struct tm	t;
	time_t		rawtime;
	char		*c;
	int			y, m, d;
#endif
	
	/* Example to allow anonymous connections based on a define */
	if (alert > 0) {
		if (ALLOW_ANON_CONNECTIONS) {
			_psTraceStr("Allowing anonymous connection for: %s.\n", 
						cert->subject.commonName);
			return SSL_ALLOW_ANON_CONNECTION;
		}
		_psTrace("Certificate callback returning fatal alert\n");
		return alert;
	}
	
#ifdef POSIX
	/* Validate the dates in the cert */
	time(&rawtime);
	localtime_r(&rawtime, &t);
	/* Localtime does months from 0-11 and (year-1900)! Normalize it. */
	t.tm_mon++;
	t.tm_year += 1900;
	
	/* Validate the 'not before' date */
	if ((c = cert->notBefore) != NULL) {
		if (strlen(c) < 8) {
			return PS_FAILURE;
		}
		/* UTCTIME, defined in 1982, has just a 2 digit year */
		if (cert->timeType == ASN_UTCTIME) {
			y =  2000 + 10 * (c[0] - '0') + (c[1] - '0'); c += 2;
		} else {
			y = 1000 * (c[0] - '0') + 100 * (c[1] - '0') + 
			10 * (c[2] - '0') + (c[3] - '0'); c += 4;
		}
		m = 10 * (c[0] - '0') + (c[1] - '0'); c += 2;
		d = 10 * (c[0] - '0') + (c[1] - '0'); 
		if (t.tm_year < y) return PS_FAILURE; 
		if (t.tm_year == y) {
			if (t.tm_mon < m) return PS_FAILURE;
			if (t.tm_mon == m && t.tm_mday < d) return PS_FAILURE;
		}
/*		_psTraceStr("Validated notBefore: %s\n", cert->notBefore); */
	}
	
	/* Validate the 'not after' date */
	if ((c = cert->notAfter) != NULL) {
		if (strlen(c) < 8) {
			return PS_FAILURE;
		}
		/* UTCTIME, defined in 1982 has just a 2 digit year */
		if (cert->timeType == ASN_UTCTIME) {
			y =  2000 + 10 * (c[0] - '0') + (c[1] - '0'); c += 2;
		} else {
			y = 1000 * (c[0] - '0') + 100 * (c[1] - '0') + 
			10 * (c[2] - '0') + (c[3] - '0'); c += 4;
		}
		m = 10 * (c[0] - '0') + (c[1] - '0'); c += 2;
		d = 10 * (c[0] - '0') + (c[1] - '0'); 
		if (t.tm_year > y) return PS_FAILURE; 
		if (t.tm_year == y) {
			if (t.tm_mon > m) return PS_FAILURE;
			if (t.tm_mon == m && t.tm_mday > d) return PS_FAILURE;
		}
/*		_psTraceStr("Validated notAfter: %s\n", cert->notAfter); */
	}
#endif /* POSIX */
	_psTraceStr("Validated cert for: %s.\n", cert->subject.commonName);
	
	return PS_SUCCESS;
}

/******************************************************************************/
/*
	Open an outgoing blocking socket connection to a remote ip and port.
	Caller should always check *err value, even if a valid socket is returned
 */
static SOCKET socketConnect(char *ip, int32 port, int32 *err)
{
	struct sockaddr_in	addr;
	SOCKET				fd;
	int32				rc;
	
	/* By default, this will produce a blocking socket */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		_psTrace("Error creating socket\n");
		*err = SOCKET_ERRNO;
		return INVALID_SOCKET;
	}
	
	memset((char *) &addr, 0x0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((short)port);
	addr.sin_addr.s_addr = inet_addr(ip);
	rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (rc < 0) {
		*err = SOCKET_ERRNO;
	} else {
		*err = 0;
	}
	return fd;
}

#else

/******************************************************************************/
/* 
    Stub main for compiling without client enabled
*/
int32 main(int32 argc, char **argv)
{
    printf("USE_CLIENT_SIDE_SSL must be enabled in matrixsslConfig.h at build" \
            " time to run this application\n");
    return -1;
}
#endif /* USE_CLIENT_SIDE_SSL */

/******************************************************************************/

