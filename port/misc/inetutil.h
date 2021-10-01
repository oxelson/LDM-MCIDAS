/*
 *	Copyright 1991 University Corporation for Atmospheric Research
 *	   Not for Direct Resale. All copies to include this notice.
 */
/* $Id: inetutil.h,v 1.1.1.1 1995/06/15 22:31:59 steve Exp $ */

/* 
 * Miscellaneous functions to make dealing with internet addresses easier.
 */

#ifndef _INETUTIL_H_
#define _INETUTIL_H_

extern char *ghostname(/* void */) ;
extern char *hostbyaddr(/* struct sockaddr_in *paddr */) ;
extern int addrbyhost(/* char *hostname, struct sockpaddr_in *paddr */) ;
extern char *s_sockaddr_in(/* struct sockaddr_in *paddr */) ;
extern int gethostaddr_in(/* struct sockaddr_in *paddr */) ;
extern int getservport(/* char *servicename, char *proto */) ;
extern int usopen(/* char *name */) ;
extern int udpopen(/* char *hostname, char *servicename */) ;

#endif /* !_INETUTIL_H_ */
