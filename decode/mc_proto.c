/*
 * $Id: mc_proto.c,v 1.6 2000/05/11 19:23:43 yoksas Exp $
 *
 * Support functions for reading a McIDAS data stream from standard input.
 */

#include <stdio.h>
#include <signal.h>
#include <assert.h>

#include <cfortran.h>
#include <ulog.h>

#include "mc_proto.h"

static char	initialized;
static unsigned	timeout	= 600;		/* should be more than sufficient */


/*
 * SIGALRM handler.
 */
    static void
alarm_handler(sig)
    int		sig;
{
    assert(SIGALRM == sig);
    uerror("SIGALRM: no data for %u seconds: returning EOF", timeout);
}


/*
 * Return a character from the input in the manner of getchar().  In
 * order to handle the situation in which we're being fed by an LDM
 * and the end of the data product is never seen, we set an alarm prior
 * to the read(2).
 */
    static int
mcgetchar()
{
    int		status;
    static char	initialized	= 0;

    if (initialized) {
	status	= 0;
    } else {
	struct sigaction	newact;
	struct sigaction	oldact;

	newact.sa_handler	= alarm_handler;
#	ifdef SA_INTERRUPT
	    newact.sa_flags	= SA_INTERRUPT;	/* for POSIX.1 semantics */
#	else
	    newact.sa_flags	= 0;
#	endif
	(void) sigemptyset(&newact.sa_mask);

	if (-1 == sigaction(SIGALRM, &newact, &oldact)) {
	    serror("mcgetchar(): can't install SIGALRM handler");
	    status	= EOF;
	} else {
	    if (SIG_DFL != oldact.sa_handler && SIG_IGN != oldact.sa_handler) {
		uerror("mcgetchar(): SIGALRM handler already installed");
		status	= EOF;
	    } else {
		initialized	= 1;
		status		= 0;
	    }
	}
    }

    if (EOF != status) {
	(void) alarm(timeout);
	status	= getchar();
    }

    return status;
}


/*
 * returns an int in range 0 to ( 2**16 -1)
 *	or EOF
 */
    static unsigned
mcgetu_sh()
{
    register int    high, low;

    if ((high = mcgetchar()) == EOF)
	return (EOF);
    if ((low = mcgetchar()) == EOF)
	return (EOF);
    return (high * 256 + low);
}

/* end of mc io stuff , low layer */


/*
 *  Gets next data packet in input stream.
 *  On return, the stream ptr is at the first character of
 *     of data and *pkt contains the packet header info.
 *  Returns the packet "Routing-code" aka "packet type code"
 *     or EOF.
 */
    int
getpacket(pkt)
    struct mcpacket *pkt;
{
    register int    ch;			/* input buffer */

    pkt->r_code = EOF;

    /* get a candidate packet header */
    if ((ch = mcgetchar()) == EOF)
	return EOF;
    if (ch < 0xf0 || ch > 0xf7) {
	unotice("Invalid start-of-packet \"0x%x\"", ch);
	return EOF;
    }

    pkt->fxub = ch;

    /* get routing code */
    if ((ch = mcgetchar()) == EOF)
	return EOF;
    if (ch > 0x32 || ch < 0) {
	/*
	 * Invalid routing-code.
	 */
	unotice("Invalid routing code 0x%02x", ch);
	return EOF;
    }						/* else */
    pkt->r_code = ch;

    /* get length bytes */
    if ((ch = mcgetu_sh()) == EOF)
	return EOF;

    /* disallow packets with zero data */
    if (ch <= 5 || ch > MAX_MC_PKTSIZE) {
	unotice("Invalid packet size %d", ch);
	return EOF;
    }						/* else */
    pkt->_cnt = pkt->length = ch - 5;

    return (pkt->r_code);
}


/*
 *  packet layer _filbuf
 *	Called when packet data has been read ,
 *      soaks up checksum and end character,
 *      gets next packet.
 */
    static int
p_mcfilbuf(pkt)
    struct mcpacket *pkt;
{
    int             ret;

    if (initialized) {
	/* get checksum */
	if ((ret = mcgetchar()) == EOF)
	    return EOF;
    } else {
	initialized	= 1;
    }

    if ((ret = getpacket(pkt)) == EOF)
	return EOF;
    if (ret != DATA_PKT) {
	return EOP;
    }
    --pkt->_cnt;
    return (mcgetchar());
}


/*
 * Return the next F0 packet, itself.  Don't use any of the `mcpacket'
 * routines before this one: the file position will be off by the checksum
 * byte.
 */
    int
getpkt(buf)
    unsigned char	*buf;
{
    register int	ch;			/* input buffer */
    int			length;
    static int count=0;

    /* get a candidate packet header */
    if ((ch = mcgetchar()) == EOF)
	return EOF;
    if (ch < 0xf0 || ch > 0xf7) {
	unotice("Invalid start-of-packet \"0x%x\"", ch);
	return EOF;
    }
    *buf++	= ch;

    /* get routing code */
    if ((ch = mcgetchar()) == EOF)
	return EOF;
    if (ch > 0x32 || ch < 0) {
	/*
	 * Invalid routing-code.
	 */
	unotice("Invalid routing code 0x%02x", ch);
	return EOF;
    }						/* else */
    *buf++	= ch;

    /* get packet length in bytes */
    if ((ch = mcgetchar()) == EOF)
	return EOF;
    *buf++	= ch;
    length	= ch * 256;
    if ((ch = mcgetchar()) == EOF)
	return EOF;
    *buf++	= ch;
    length	+= ch;

    /* disallow invalid length packets */
    if (length < 5 || length > MAX_MC_PKTSIZE) {
	unotice("Invalid packet size %d", length);
	return EOF;
    }						/* else */

    /*
     * Get rest of packet.
     */
    length	-= 4;
    while (length-- > 0) {
	if ((ch = mcgetchar()) == EOF)
	    return EOF;
	*buf++	= ch;
    }

    count++;
    return 0;
}


/*
 * McIDAS interface to the above function.
 */
    void
c_getrec(buf)
    int		*buf;
{
    static unsigned char	trailer_packet[5]	= {0xf0, 1, 0, 5, 6};

    if (getpkt((unsigned char*)buf) == EOF)
	(void) memcpy((void*)buf, (char*)trailer_packet, 
		      sizeof(trailer_packet));
}


/*
 * FORTRAN interface to the above function.
 */
FCALLSCSUB1(c_getrec,GETREC,getrec,PINT)


/*
 * like getc(), recursion
 */
#define mcgetc(pkt) (--(pkt)->_cnt >=0 ? mcgetchar() : p_mcfilbuf(pkt))


/*
 * like fgetc
 *    externally visible function
 */
    int
mcfgetc(pkt)
    struct mcpacket *pkt;
{
    return (mcgetc(pkt));
}


/*
 * mcread (like fread)
 * -- this routine reads code from pc-mcidas stream and takes
 * care of escaped characters.
 *	Reads into the array 'buf' at most 'count' objects of size 'size'.
 *	Returns number of objects read or 0 on error.
 */
    int
mcread(buf, size, count, pkt)
    register unsigned char *buf;	/* pointer to data storage area */
    int             size;
    int             count;		/* number of requested data elements */
    struct mcpacket *pkt;
{
    register int    ch;
    int             ret;
    int             ss;

    ret = 0;
    if (size)
	for (; ret < count; ret++) {
	    ss = size;
	    do {
		if ((ch = mcgetc(pkt)) >= 0)
		    *buf++ = ch;
		else
		    return (ret);
	    } while (--ss);
    } else {
	unotice("mcread zero sized objects?");
	return (count);
    }
    return (ret);
}


/*
 * mcskip
 *	Skip at most 'count' objects of size 'size' from mcidas input.
 *	Returns number of objects read or 0 on error.
 */
    int
mcskip(size, count, pkt)
    int             size;
    int             count;		/* number of requested data elements */
    struct mcpacket *pkt;
{
    register int    ch;
    int             ret;
    int             ss;

    ret = 0;
    if (size)
	for (; ret < count; ret++) {
	    ss = size;
	    do {
		if ((ch = mcgetc(pkt)) < 0)
		    return (ret);
	    } while (--ss);
    } else {
	unotice("mcskip zero sized objects?");
	return (count);
    }
    return (ret);
}


/*
 *  Look for 8 byte sync pattern in data.
 *	return EOP, EOF or the byte offset to reach registration
 *   EG, a return of 0 is normal.
 */
    int
getsync(pkt)
    struct mcpacket *pkt;
{
    register int    ch;
    int             skipped;

    for (skipped = 0; skipped < 1028; skipped++) {
	/* one */
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0x00)
	    continue;
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0xff)
	    continue;
	/* two */
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0x00)
	    continue;
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0xff)
	    continue;
	/* three */
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0x00)
	    continue;
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0xff)
	    continue;
	/* four */
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0x00)
	    continue;
	if ((ch = mcgetc(pkt)) < 0)
	    return ch;
	if (ch != 0xff)
	    continue;
	return (skipped);
    }
    return (skipped);
}


/*
 * Copy data from the mcidas stream to a file
 *	returns the number of bytes copied
 */
    int
mc_datacopy(pkt, ofp, nbytes)
    struct mcpacket *pkt;
    FILE           *ofp;
    int             nbytes;
{
    register int    ch;
    int             copied;

    for (copied = 0; copied < nbytes; copied++) {
	if ((ch = mcgetc(pkt)) < 0)
	    break;
	if (putc(ch, ofp) == EOF)
	    break;
    }
    return (copied);
}


#ifdef SWAP_OUT
/*
 * convert from little endian to big endian four byte object
 *  or vis versa
 */
    unsigned long
swaplong(lend)
    unsigned long          lend;
{
    unsigned long          bend = 0;
    unsigned char         *lp, *bp;

    lp = ((unsigned char *) & lend) + 3;
    bp = (unsigned char *) & bend;

    *bp++ = *lp--;
    *bp++ = *lp--;
    *bp++ = *lp--;
    *bp = *lp;

    return (bend);
}

#endif


/*
 * Word oriented copy data from the mcidas stream to a file
 * Swaps bytes if SWAP_BYTE #defined
 *	returns the number of words copied
 */
    int
mc_wcopy(pkt, ofp, nwords)
    struct mcpacket *pkt;
    FILE           *ofp;
    int             nwords;
{
    register int    ch;
    int             copied;
    int             ss;
    unsigned char  *wp;
    unsigned long   theWord;

    for (copied = 0; copied < nwords; copied++) {
	ss = 4;
	theWord = 0;
	wp = (unsigned char *) &theWord;
	do {
	    if ((ch = mcgetc(pkt)) < 0)
		return copied;
	    *wp++ = ch;
	} while (--ss);

	if (mc_putw(theWord, ofp) == EOF)
	    return (copied);
    }
    return (copied);
}


/*
 * returns an int in range 0 to ( 2**16 -1),
 *	EOF, or EOP
 */
    int
mcgetsh(pkt)
    struct mcpacket *pkt;
{
    register int    high, low;

    if ((high = mcgetc(pkt)) < 0)
	return (high);
    if ((low = mcgetc(pkt)) < 0)
	return (low);
    return (high * 256 + low);
}


/*
 * returns zero and set *wordp to a number between 0 and (2**32 -1), or
 *	EOF, or EOP
 */
    int
mcgetw(pkt, wordp)
    struct mcpacket *pkt;
    unsigned long  *wordp;
{
    int             hh, hl, lh, ll;

    if ((hh = mcgetc(pkt)) < 0)
	return (hh);
    if ((hl = mcgetc(pkt)) < 0)
	return (hl);
    if ((lh = mcgetc(pkt)) < 0)
	return (lh);
    if ((ll = mcgetc(pkt)) < 0)
	return (ll);
    *wordp = ll + 256 * lh + 65536 * hl + 16777216 * hh;
    return (0);
}


/*
 * Returns next nibble, EOF or EOP
 */
    int
mcgetnib(pkt, lo)
    struct mcpacket *pkt;
    int             lo;			/* if 1, return low order nibble */
{
    /* high, low, high ....*/

    static int      nibbuf;

    if (lo) {
	return (nibbuf & 0xf);
    }						/* else */
    nibbuf = mcgetc(pkt);
    if (nibbuf < 0) {
	return nibbuf;
    }						/* else */
    return ((nibbuf >> 4) & 0xf);
}
