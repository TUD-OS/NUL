/*
 *	server.c
 *  Release $Name: MATRIXSSL-3-2-2-OPEN $
 *
 *	Non-blocking MatrixSSL server example supporting multiple connections
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

#include "app.h"
#include "matrixssl/matrixsslApi.h"

#ifdef USE_SERVER_SIDE_SSL

#include <signal.h>         /* Defines SIGTERM, etc. */

#ifdef WIN32
#pragma message("DO NOT USE THESE DEFAULT KEYS IN PRODUCTION ENVIRONMENTS.")
#else
#warning "DO NOT USE THESE DEFAULT KEYS IN PRODUCTION ENVIRONMENTS."
#endif


#define USE_HEADER_KEYS

#ifdef USE_HEADER_KEYS
#include "sampleCerts/certSrv.h"
#include "sampleCerts/privkeySrv.h"
#else
static char certSrvFile[] = "certSrv.pem";
static char privkeySrvFile[] = "privkeySrv.pem";
#endif /* USE_HEADER_KEYS */


/********************************** Defines ***********************************/

#define SSL_TIMEOUT			45000
#define SELECT_TIME			1000

#define	GOTO_SANITY			32	/* Must be <= 255 */
/*
	The ACCEPT_QUEUE is an optimization mechanism that allows the server to
	accept() up to this many connections before serving any of them.  The
	reason is that the timeout waiting for the accept() is much shorter
	than the timeout for the actual processing. 
*/
#define	ACCEPT_QUEUE		16	

/********************************** Globals ***********************************/

static DLListEntry		g_conns;
static int32			g_exitFlag;
static unsigned char	g_httpResponseHdr[] = "HTTP/1.0 200 OK\r\n"
	"Server: PeerSec Networks MatrixSSL/" MATRIXSSL_VERSION "\r\n"
	"Pragma: no-cache\r\n"
	"Cache-Control: no-cache\r\n"
	"Content-type: text/plain\r\n"
	"Content-length: 9\r\n"
	"\r\n"
	"MatrixSSL";


/****************************** Local Functions *******************************/

static int32 selectLoop(sslKeys_t *keys, SOCKET lfd);
static int32 httpWriteResponse(ssl_t *cp);
static void setSocketOptions(SOCKET fd);
static SOCKET socketListen(short port, int32 *err);
static void closeConn(httpConn_t *cp, int32 reason);

#ifdef POSIX
static void sigsegv_handler(int i);
static void sigintterm_handler(int i);
static int32 sighandlers(void);
#endif /* POSIX */


#define certCb NULL

/******************************************************************************/
/*
	Non-blocking socket event handler
	Wait one time in select for events on any socket
	This will accept new connections, read and write to sockets that are
	connected, and close sockets as required.
 */
static int32 selectLoop(sslKeys_t *keys, SOCKET lfd)
{
	httpConn_t		*cp;
	psTime_t		now;
	DLListEntry		connsTmp;
	DLListEntry		*pList;
	
	fd_set			readfd, writefd;
	struct timeval	timeout;
	SOCKET			fd, maxfd;
	
	unsigned char	*buf;
	int32			rc, len, transferred, val;
	unsigned char	rSanity, wSanity, acceptSanity;
	
	DLListInit(&connsTmp);
	rc = PS_SUCCESS;
	maxfd = INVALID_SOCKET;
	timeout.tv_sec = SELECT_TIME / 1000;
	timeout.tv_usec = (SELECT_TIME % 1000) * 1000;
	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
	
	/* Always set readfd for listening socket */
	FD_SET(lfd, &readfd);
	if (lfd > maxfd) {
		maxfd = lfd;
	}
/*	
	Check timeouts and set readfd and writefd for connections as required.
	We use connsTemp so that removal on error from the active iteration list
		doesn't interfere with list traversal 
 */
	psGetTime(&now);
	while (!DLListIsEmpty(&g_conns)) {
		pList = DLListGetHead(&g_conns);
		cp = DLListGetContainer(pList, httpConn_t, List);
		DLListInsertTail(&connsTmp, &cp->List);
		/*	If timeout != 0 msec ith no new data, close */
		if (cp->timeout && (psDiffMsecs(cp->time, now) > (int32)cp->timeout)) {
			closeConn(cp, PS_TIMEOUT_FAIL);
			continue;	/* Next connection */
		}
		/* Always select for read */
		FD_SET(cp->fd, &readfd);
		/* Select for write if there's pending write data or connection */
		if (matrixSslGetOutdata(cp->ssl, NULL) > 0) {
			FD_SET(cp->fd, &writefd);
		}
		/* Housekeeping for maxsock in select call */
		if (cp->fd > maxfd) {
			maxfd = cp->fd;
		}
	}
	
	/* Use select to check for events on the sockets */
	if ((val = select(maxfd + 1, &readfd, &writefd, NULL, &timeout)) <= 0) {
		/* On error, restore global connections list */
		while (!DLListIsEmpty(&connsTmp)) {
			pList = DLListGetHead(&connsTmp);
			cp = DLListGetContainer(pList, httpConn_t, List);
			DLListInsertTail(&g_conns, &cp->List);
		}
		/* Select timeout */
		if (val == 0) {
			return PS_TIMEOUT_FAIL;
		}
		/* Woke due to interrupt */
		if (SOCKET_ERRNO == EINTR) {
			return PS_TIMEOUT_FAIL;
		}
		/* Should attempt to handle more errnos, such as EBADF */
		return PS_PLATFORM_FAIL;
	}
	
	/* Check listener for new incoming socket connections */
	if (FD_ISSET(lfd, &readfd)) {
		for (acceptSanity = 0; acceptSanity < ACCEPT_QUEUE; acceptSanity++) {
			fd = accept(lfd, NULL, NULL);
			if (fd == INVALID_SOCKET) {
				break;	/* Nothing more to accept; next listener */
			}
			setSocketOptions(fd);
			cp = malloc(sizeof(httpConn_t));
			if ((rc = matrixSslNewServerSession(&cp->ssl, keys, certCb)) < 0) {
				close(fd); fd = INVALID_SOCKET;
				continue;
			}
			cp->fd = fd;
			fd = INVALID_SOCKET;
			cp->timeout = SSL_TIMEOUT;
			psGetTime(&cp->time);
			cp->parsebuf = NULL;
			cp->parsebuflen = 0;
			DLListInsertTail(&connsTmp, &cp->List);
			/* Fake that there is read data available, no harm if there isn't */
			FD_SET(cp->fd, &readfd);
/*			_psTraceInt("=== New Client %d ===\n", cp->fd); */
		}
	}
	
	/* Check each connection for read/write activity */
	while (!DLListIsEmpty(&connsTmp)) {
		pList = DLListGetHead(&connsTmp);
		cp = DLListGetContainer(pList, httpConn_t, List);
		DLListInsertTail(&g_conns, &cp->List);
		
		rSanity = wSanity = 0;
/*
		See if there's pending data to send on this connection
		We could use FD_ISSET, but this is more reliable for the current
			state of data to send.
 */
WRITE_MORE:
		if ((len = matrixSslGetOutdata(cp->ssl, &buf)) > 0) {
			/* Could get a EWOULDBLOCK since we don't check FD_ISSET */
			transferred = send(cp->fd, buf, len, MSG_DONTWAIT);
			if (transferred <= 0) {
#ifdef WIN32
				if (SOCKET_ERRNO != EWOULDBLOCK &&
					SOCKET_ERRNO != WSAEWOULDBLOCK) {

#else
				if (SOCKET_ERRNO != EWOULDBLOCK) {
#endif
					closeConn(cp, PS_PLATFORM_FAIL);
					continue;	/* Next connection */
				}
			} else {
				/* Indicate that we've written > 0 bytes of data */
				if ((rc = matrixSslSentData(cp->ssl, transferred)) < 0) {
					closeConn(cp, PS_ARG_FAIL);
					continue;	/* Next connection */
				}
				if (rc == MATRIXSSL_REQUEST_CLOSE) {
					closeConn(cp, MATRIXSSL_REQUEST_CLOSE);
					continue;	/* Next connection */
				} else if (rc == MATRIXSSL_HANDSHAKE_COMPLETE) {
					/* If the protocol is server initiated, send data here */
				}
				/* Update activity time */
				psGetTime(&cp->time);
				/* Try to send again if more data to send */
				if (rc == MATRIXSSL_REQUEST_SEND || transferred < len) {
					if (wSanity++ < GOTO_SANITY) goto WRITE_MORE;
				}
			}
		} else if (len < 0) {
			closeConn(cp, PS_ARG_FAIL);
			continue;	/* Next connection */
		}
		
/*
		Check the file descriptor returned from select to see if the connection
		has data to be read
 */
		if (FD_ISSET(cp->fd, &readfd)) {
READ_MORE:
			/* Get the ssl buffer and how much data it can accept */
			/* Note 0 is a return failure, unlike with matrixSslGetOutdata */
			if ((len = matrixSslGetReadbuf(cp->ssl, &buf)) <= 0) {
				closeConn(cp, PS_ARG_FAIL);
				continue;	/* Next connection */
			}
			if ((transferred = recv(cp->fd, buf, len, MSG_DONTWAIT)) < 0) {
				/* We could get EWOULDBLOCK despite the FD_ISSET on goto  */
#ifdef WIN32
				if (SOCKET_ERRNO != EWOULDBLOCK &&
					SOCKET_ERRNO != WSAEWOULDBLOCK) {

#else
				if (SOCKET_ERRNO != EWOULDBLOCK) {
#endif
					closeConn(cp, PS_PLATFORM_FAIL);
				}
				continue;	/* Next connection */
			}
			/* If EOF, remote socket closed. This is semi-normal closure.
			   Officially, we should close on closure alert. */
			if (transferred == 0) {
/*				psTraceIntInfo("Closing connection %d on EOF\n", cp->fd); */
				closeConn(cp, 0);
				continue;	/* Next connection */
			}
/*
			Notify SSL state machine that we've received more data into the
			ssl buffer retreived with matrixSslGetReadbuf.
 */
			if ((rc = matrixSslReceivedData(cp->ssl, (int32)transferred, &buf, 
											(uint32*)&len)) < 0) {
				closeConn(cp, 0);
				continue;	/* Next connection */
			}
			/* Update activity time */
			psGetTime(&cp->time);
			
PROCESS_MORE:
			/* Process any incoming plaintext application data */
			switch (rc) {
				case MATRIXSSL_HANDSHAKE_COMPLETE:
					/* If the protocol is server initiated, send data here */
					goto READ_MORE;
				case MATRIXSSL_APP_DATA:
					/* Remember, must handle if len == 0! */
					if ((rc = httpBasicParse(cp, buf, len)) < 0) {
						_psTrace("Couldn't parse HTTP data.  Closing conn.\n");
						closeConn(cp, PS_PROTOCOL_FAIL);
						continue; /* Next connection */
					}
					if (rc == HTTPS_COMPLETE) {
						if (httpWriteResponse(cp->ssl) < 0) {
							closeConn(cp, PS_PROTOCOL_FAIL);
							continue; /* Next connection */
						}
						/* For HTTP, we assume no pipelined requests, so we 
						 close after parsing a single HTTP request */
						/* Ignore return of closure alert, it's optional */
						matrixSslEncodeClosureAlert(cp->ssl);
						rc = matrixSslProcessedData(cp->ssl, &buf, (uint32*)&len);
						if (rc > 0) {
							/* Additional data is available, but we ignore it */
							_psTrace("HTTP data parsing not supported, ignoring.\n");
							closeConn(cp, PS_SUCCESS);
							continue; /* Next connection */
						} else if (rc < 0) {
							closeConn(cp, PS_PROTOCOL_FAIL);
							continue; /* Next connection */
						}
						/* rc == 0, write out our response and closure alert */
						goto WRITE_MORE;
					}
					/* We processed a partial HTTP message */
					if ((rc = matrixSslProcessedData(cp->ssl, &buf, (uint32*)&len)) == 0) {
						goto READ_MORE;
					}
					goto PROCESS_MORE;
				case MATRIXSSL_REQUEST_SEND:
					/* Prevent us from reading again after the write,
					 although that wouldn't be the end of the world */
					FD_CLR(cp->fd, &readfd);
					if (wSanity++ < GOTO_SANITY) goto WRITE_MORE;
					break;
				case MATRIXSSL_REQUEST_RECV:
					if (rSanity++ < GOTO_SANITY) goto READ_MORE; 
					break;
				case MATRIXSSL_RECEIVED_ALERT:
					/* The first byte of the buffer is the level */
					/* The second byte is the description */
					if (*buf == SSL_ALERT_LEVEL_FATAL) {
						psTraceIntInfo("Fatal alert: %d, closing connection.\n", 
									*(buf + 1));
						closeConn(cp, PS_PROTOCOL_FAIL);
						continue; /* Next connection */
					}
					/* Closure alert is normal (and best) way to close */
					if (*(buf + 1) == SSL_ALERT_CLOSE_NOTIFY) {
						closeConn(cp, PS_SUCCESS);
						continue; /* Next connection */
					}
					psTraceIntInfo("Warning alert: %d\n", *(buf + 1));
					if ((rc = matrixSslProcessedData(cp->ssl, &buf, (uint32*)&len)) == 0) {
						/* No more data in buffer. Might as well read for more. */
						goto READ_MORE;
					}
					goto PROCESS_MORE;

				default:
					/* If rc <= 0 we fall here */
					closeConn(cp, PS_PROTOCOL_FAIL);
					continue; /* Next connection */
			}
			/* Always try to read more if we processed some data */
			if (rSanity++ < GOTO_SANITY) goto READ_MORE;
		} /*  readfd handling */
	}	/* connection loop */
	return PS_SUCCESS;
}

/******************************************************************************/
/*
	Create an HTTP response and encode it to the SSL buffer
 */
#define	TEST_SIZE	16000
static int32 httpWriteResponse(ssl_t *cp)
{
	unsigned char	*buf;
	uint32			available;
	
	if ((available = matrixSslGetWritebuf(cp, &buf, 
							strlen((char *)g_httpResponseHdr) + 1)) < 0) {
		return PS_MEM_FAIL;
	}
	strncpy((char *)buf, (char *)g_httpResponseHdr, available);
	if (matrixSslEncodeWritebuf(cp, strlen((char *)buf)) < 0) {
		return PS_MEM_FAIL;
	}
	return MATRIXSSL_REQUEST_SEND;
}

/******************************************************************************/
/*
	Main non-blocking SSL server
	Initialize MatrixSSL and sockets layer, and loop on select
 */
int32 main(int32 argc, char **argv)
{
	sslKeys_t		*keys;
	SOCKET			lfd;
	int32			err, rc;
#ifdef WIN32
	WSADATA			wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif


	keys = NULL;
	DLListInit(&g_conns);
	g_exitFlag = 0;
	lfd = INVALID_SOCKET;
	
#ifdef POSIX
	if (sighandlers() < 0) {
		return PS_PLATFORM_FAIL;
	}
#endif	/* POSIX */
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
	if ((rc = matrixSslLoadRsaKeysMem(keys, certSrvBuf, sizeof(certSrvBuf),
				   privkeySrvBuf, sizeof(privkeySrvBuf), NULL, 0)) < 0) {
		_psTrace("No certificate material loaded.  Exiting\n");
		matrixSslDeleteKeys(keys);
		matrixSslClose();
		return rc;
	}
#else /* USE_HEADER_KEYS */
/*
	File based keys
*/
	if ((rc = matrixSslLoadRsaKeys(keys, certSrvFile, privkeySrvFile, NULL,
			NULL)) < 0) {
		_psTrace("No certificate material loaded.  Exiting\n");
		matrixSslDeleteKeys(keys);
		matrixSslClose();
		return rc;
	}
#endif /* USE_HEADER_KEYS */


	/* Create the listening socket that will accept incoming connections */
	if ((lfd = socketListen(HTTPS_PORT, &err)) == INVALID_SOCKET) {
		_psTraceInt("Can't listen on port %d\n", HTTPS_PORT);
		goto L_EXIT;
	}

	/* Main select loop to handle sockets events */
	while (!g_exitFlag) {
		selectLoop(keys, lfd);
	}

L_EXIT:
	if (lfd != INVALID_SOCKET) close(lfd);
	if (keys) matrixSslDeleteKeys(keys);
	matrixSslClose();
	
	return 0;
}

/******************************************************************************/
/*
	Close a socket and free associated SSL context and buffers
 */
static void closeConn(httpConn_t *cp, int32 reason)
{
	unsigned char	*buf;
	int32			len;
	
	DLListRemove(&cp->List);
	/* Quick attempt to send a closure alert, don't worry about failure */
	if (matrixSslEncodeClosureAlert(cp->ssl) >= 0) {
		if ((len = matrixSslGetOutdata(cp->ssl, &buf)) > 0) {
			if ((len = send(cp->fd, buf, len, MSG_DONTWAIT)) > 0) {
				matrixSslSentData(cp->ssl, len);
			}
		}
	}
	if (cp->parsebuf != NULL) {
		psAssert(cp->parsebuflen > 0);
		free(cp->parsebuf);
		cp->parsebuflen = 0;
	}
	matrixSslDeleteSession(cp->ssl);
	if (cp->fd != INVALID_SOCKET) {
		close(cp->fd);
	}
	if (reason >= 0) {
/*		_psTraceInt("=== Closing Client %d ===\n", cp->fd); */
	} else {
		_psTraceInt("=== Closing Client %d on Error ===\n", cp->fd);
	}
	free(cp);
}

/******************************************************************************/
/*
	Establish a listening socket for incomming connections
 */
static SOCKET socketListen(short port, int32 *err)
{
	struct sockaddr_in	addr;
	SOCKET				fd;
	
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		_psTrace("Error creating listen socket\n");
		*err = SOCKET_ERRNO;
		return INVALID_SOCKET;
	}
	
	setSocketOptions(fd);
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		_psTrace("Can't bind socket. Port in use or insufficient privilege\n");
		*err = SOCKET_ERRNO;
		return INVALID_SOCKET;
	}
	if (listen(fd, SOMAXCONN) < 0) {
		_psTrace("Error listening on socket\n");
		*err = SOCKET_ERRNO;
		return INVALID_SOCKET;
	}
	_psTraceInt("Listening on port %d\n", port);
	return fd;
}

/******************************************************************************/
/*
	Make sure the socket is not inherited by exec'd processes
	Set the REUSE flag to minimize the number of sockets in TIME_WAIT
	Then we set REUSEADDR, NODELAY and NONBLOCK on the socket
*/
static void setSocketOptions(SOCKET fd)
{
	int32 rc;

#ifdef POSIX
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
	rc = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&rc, sizeof(rc));
#ifdef POSIX
	rc = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&rc, sizeof(rc));
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#elif defined(WIN32)
	rc = 1;		/* 1 for non-block, 0 for block */
    ioctlsocket(fd, FIONBIO, &rc);
#endif
#ifdef __APPLE__  /* MAC OS X */
    rc = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&rc, sizeof(rc));
#endif
}

#ifdef POSIX
/******************************************************************************/
/*
	Handle some signals on POSIX platforms
	Lets ctrl-c do a clean exit of the server.
 */
static int32 sighandlers(void)
{
	if (signal(SIGINT, sigintterm_handler) == SIG_ERR ||
			signal(SIGTERM, sigintterm_handler) == SIG_ERR ||
			signal(SIGPIPE, SIG_IGN) == SIG_ERR ||
			signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		return PS_PLATFORM_FAIL;
	}
	return 0;
}

/* Warn on segmentation violation */
static void sigsegv_handler(int unused)
{
	printf("Segfault! Please report this as a bug to support@peersec.com\n");
	exit(EXIT_FAILURE);
}

/* catch ctrl-c or sigterm */
static void sigintterm_handler(int unused)
{
	g_exitFlag = 1; /* Rudimentary exit flagging */
	printf("Exiting due to interrupt.\n");
}
#endif /* POSIX */


#else

/******************************************************************************/
/*
    Stub main for compiling without server enabled
*/
int32 main(int32 argc, char **argv)
{
    printf("USE_SERVER_SIDE_SSL must be enabled in matrixsslConfig.h at build" \
            " time to run this application\n");
    return -1;
}
#endif /* USE_SERVER_SIDE_SSL */

/******************************************************************************/

