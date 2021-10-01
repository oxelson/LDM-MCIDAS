/*
 *   Copyright 1993, University Corporation for Atmospheric Research
 *   See ../COPYRIGHT file for copying and redistribution conditions.
 */
/* $Id: ulog.h,v 1.1.1.1 1995/06/15 22:32:01 steve Exp $ */

#ifndef _ULOG_H_
#define _ULOG_H_
#include <syslog.h>

/*
 * options to openulog() which are not options
 * to openlog()
 * Make sure these dont collide with ones in <syslog.h>
 * or the 4.2 compatibility section below
 */
#define LOG_NOTIME 0x200		/* don't put on the timestamp */
#define LOG_LOCALTIME 0x100  /* use localtime. default is gmt */

/*
 * This set of #defines allows this to work even with a
 * 4.2 BSD style syslog.h, like on Ultrix 4.x
 */
#ifndef LOG_NFACILITIES
/* means this system doesn't have 4.3 BSD syslog */
#define LOG_NFACILITIES 0
#endif /* !LOG_NFACILITIES */

#ifndef LOG_PRIMASK
#define LOG_PRIMASK (LOG_EMERG | LOG_ALERT | LOG_CRIT | LOG_ERR | LOG_WARNING \
	| LOG_NOTICE | LOG_INFO | LOG_DEBUG)
#endif

#ifndef LOG_FACMASK
#define LOG_FACMASK (~LOG_PRIMASK)
#endif

#ifndef LOG_USER
#define LOG_USER 0
#endif

#ifndef LOG_LOCAL0
#define LOG_LOCAL0 0
#endif

#ifndef LOG_CONS
#define LOG_CONS 0x20
#endif

#ifndef LOG_NOWAIT
#define LOG_NOWAIT 0x40
#endif

#ifndef LOG_MASK
#define	LOG_MASK(pri)	(1 << (pri))		/* mask for one priority */
#endif 

#ifndef LOG_UPTO
#define	LOG_UPTO(pri)	((1 << ((pri)+1)) - 1)	/* all priorities through pri */
#endif 
/* End 4.2 compatiblity section */


/*
 * The "facility" used by ldm applications.
 */
#ifndef LOG_LDM
#define LOG_LDM LOG_LOCAL0
#endif


#ifdef __cplusplus
extern "C" int closeulog(void) ;
extern "C" int openulog(
	const char *ident ,
	int options ,
	int facility , 
	const char *logfilename) ;
extern "C" int ulog(int pri, const char *fmt, ...) ;
extern "C" int setulogmask(int pmask) ;
extern "C" int toggleulogpri(int pri) ;
extern "C" void serror(const char *fmt, ...) ;
extern "C" void uerror(const char *fmt, ...) ;
extern "C" void unotice(const char *fmt, ...) ;
extern "C" void uinfo(const char *fmt, ...) ;
extern "C" void udebug(const char *fmt, ...) ;
extern "C" char *basename(char *av0) ;
extern "C" void _uassert( const char *ex, const char *file, int line) ;
#elif defined(__STDC__) || defined(STDC_ARGS)
extern int closeulog(void) ;
extern int openulog(
	const char *ident ,
	int options ,
	int facility , 
	const char *logfilename) ;
extern int ulog(int pri, const char *fmt, ...) ;
extern int setulogmask(int pmask) ;
extern int toggleulogpri(int pri) ;
extern void serror(const char *fmt, ...) ;
extern void uerror(const char *fmt, ...) ;
extern void unotice(const char *fmt, ...) ;
extern void uinfo(const char *fmt, ...) ;
extern void udebug(const char *fmt, ...) ;
extern char *basename(char *av0) ;
extern void _uassert( const char *ex, const char *file, int line) ;
#else /* Old Style C */
extern int closeulog() ;
extern int openulog() ;
extern int ulog() ;
extern int setulogmask() ;
extern int toggleulogpri() ;
extern void serror() ;
extern void uerror() ;
extern void unotice() ;
extern void uinfo() ;
extern void udebug() ;
extern char *basename() ;
extern void _uassert() ;
#endif

/*
 * When we are using ulog, we want assert() messages to go via
 * the logger.
 */
#if defined(assert) && !defined(NDEBUG)
#undef assert
#if defined(__STDC__) || defined(__cplusplus)
#       define assert(EX) \
            (((int) (EX)) ? (void)0 :  _uassert(#EX, __FILE__, __LINE__))
#else
#       define assert(EX) \
            (((int) (EX)) ? (void)0 :  _uassert("EX", __FILE__, __LINE__))
#endif
#endif


#ifdef NO_STRERROR
extern char *   strerror(/* int */);
#endif

#endif /* !_ULOG_H_ */
