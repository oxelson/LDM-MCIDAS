/*
 *	Copyright 1991, University Corporation for Atmospheric Research.
 *	   Not for Direct Resale. All copies to include this notice.
 */
/* $Id: inetutil.c,v 1.3 1996/03/22 19:54:46 steve Exp $ */

/* 
 * Miscellaneous functions to make dealing with internet addresses easier.
 */

/*LINTLIBRARY*/

#if defined(_AIX) && !defined(_ALL_SOURCE)
#   define _ALL_SOURCE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/param.h> /* MAXHOSTNAMELEN */
#include <string.h>
#include "inetutil.h"


/*
 * convenience wrapper around gethostname(2)
 * !NO_INET_FQ_KLUDGE ==> try to make sure it is "fully qualified"
 */
char *
ghostname()
{
	static char hostname[MAXHOSTNAMELEN+1] ;
	if(gethostname(hostname, MAXHOSTNAMELEN) < 0)
		return NULL ;
#ifndef NO_INET_FQ_KLUDGE
	if(strchr(hostname, '.') == NULL)
	{
		/* gethostname return not fully qualified */
		struct hostent *hp ;
		hp = gethostbyname(hostname) ;
		if(hp == NULL || hp->h_addrtype != AF_INET) 
		{
			return hostname ;
		}
		/* else, hope hp->h_name is fully qualified */
		return hp->h_name ;
	}
#endif
	return hostname ;
}


/*
 * Return a string identifying the internet host referred to
 * by paddr. If the hostname lookup fails, returns "dotted quad"
 * form of the address.
 */
char *
hostbyaddr(paddr)
struct sockaddr_in *paddr ;
{
	struct hostent *hp ;
	char *otherguy = NULL ;

	hp = gethostbyaddr((char *) &paddr->sin_addr,
		sizeof (paddr->sin_addr), AF_INET) ;
	if(hp != NULL)
		otherguy = hp->h_name ;
	else
		otherguy = inet_ntoa(paddr->sin_addr) ;
	return otherguy ;
}


/*
 * get the sockaddr_in corresponding to 'hostname' (name or NNN.NNN.NNN.NNN)
 */
int
addrbyhost(hostname, paddr)
char *hostname ;
struct sockaddr_in *paddr ; /* modified on return */
{
	
	paddr->sin_addr.s_addr = inet_addr(hostname) ; /* handle number form */
	if((int) (paddr->sin_addr.s_addr) == -1)
	{
		struct hostent *hp ;
		hp = gethostbyname(hostname) ;
		if(hp == NULL || hp->h_addrtype != AF_INET) 
		{
			return -1 ;
		}
		memmove((char *)&paddr->sin_addr, hp->h_addr, hp->h_length) ;
	}
	paddr->sin_family= AF_INET ;
	paddr->sin_port= 0 ;
	memset(paddr->sin_zero, 0, sizeof (paddr->sin_zero)) ;
	return 0 ;
}


char *
s_sockaddr_in(paddr)
struct sockaddr_in *paddr ;
{
	static char buf[64] ;
	(void) sprintf(buf,
		"sin_port %5d, sin_addr %s",
		paddr->sin_port,
		inet_ntoa(paddr->sin_addr)) ;
	return buf ;
}


/*
 * Puts the address of the current host into *paddr
 * Returns 0 on success, -1 failure
 */
int
gethostaddr_in(paddr)
struct sockaddr_in *paddr ;
{
	char hostname[MAXHOSTNAMELEN] ;
	struct hostent *hp ;

	if(gethostname(hostname,MAXHOSTNAMELEN) == -1)
		return -1 ;

	hp = gethostbyname(hostname) ;
	if(hp == NULL)
		return -1 ;
	
	(void) memmove(&paddr->sin_addr, hp->h_addr, hp->h_length) ;

	return 0 ;
}


/*
 * Return the well known port for (servicename, proto)
 * or -1 on failure.
 */
int
getservport(servicename, proto)
char *servicename ;
char *proto ;
{
	struct servent *se ;
	se = getservbyname(servicename, proto) ;
	if(se == NULL)
		return -1 ;
	/* else */
	return se->s_port ;
}


/*
 * Attempt to connect to a unix domain socket.
 * Create & connect.
 * Returns (socket) descriptor or -1 on error.
 */
int
usopen(name)
char *name ; /* name of socket */
{
	int sock = -1 ;
	struct sockaddr addr ;	/* AF_UNIX address */

	sock = socket(AF_UNIX, SOCK_DGRAM, 0) ;
	if(sock == -1)
		return -1 ;
	/* else */

	addr.sa_family = AF_UNIX;
	(void) strncpy(addr.sa_data, name, sizeof(addr.sa_data)) ;

	if (connect(sock, &addr, sizeof(addr)) == -1)
	{
		(void) close(sock) ;
		return -1 ;
	}
	/* else */

	return sock ;
}


/*
 * Attempt to connect to a internet domain udp socket.
 * Create & connect.
 * Returns (socket) descriptor or -1 on error.
 */
int
udpopen(hostname, servicename)
char *hostname ;
char *servicename ;
{
	int sock = -1 ;
	int port = -1 ;
	struct sockaddr_in addr ;	/* AF_INET address */

	sock = socket(AF_INET, SOCK_DGRAM, 0) ;
	if(sock == -1)
		return -1 ;
	/* else */

	if(addrbyhost(hostname, &addr) == -1)
	{
		(void) close(sock) ;
		return -1 ;
	}
	/* else */

	if((port = (short) getservport(servicename, "udp")) == -1)
	{
		(void) close(sock) ;
		return -1 ;
	}
	/* else */
	addr.sin_port = port ;

	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		(void) close(sock) ;
		return -1 ;
	}
	/* else */

	return sock ;
}
