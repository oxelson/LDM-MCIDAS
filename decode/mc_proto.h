/* $Id: mc_proto.h,v 1.2 1993/09/07 15:56:39 steve Exp $ */
/*
 * mcidas.h -- This file defines common things for decoding pc-mcidas
 * data streams.
 */

#if defined(mc68000) || defined(sparc) || defined(_IBMR2)
#define BIGENDIAN
/* 
 * Define this if you have PC-McIdas running off the files nabbed by this
 * program.  On output, need to swap to the pc byte order.
 */
#define SWAP_OUT
#endif

#ifdef SWAP_OUT
#define mc_putw(w,fp)	putw(swaplong(w),(fp))
#else
#define mc_putw(w,fp)	putw((w),(fp))
#endif

#define MAX_MC_PKTSIZE 767
#define DATAMAX MAX_MC_PKTSIZE

/* structure containing packet information */
struct mcpacket {
  int _cnt ;                    /* unread bytes in packet */
  unsigned char fxub;		/* packet header */
  int r_code;			/* routing code */
  int length;			/* computed length in bytes of data */
};

/*
 *	"Routing-codes" aka "packet type codes"
 */
#define  HEADER_PKT	0x00
#define  TRAILER_PKT	0x01
#define  A_KEYIN_PKT	0x10
#define  DATA_PKT	0x20


/*
 * We need a "End of Product" flag which is not so fatal as the
 *    "End of File"
 */
#define EOP (EOF - 1)	/* Does this make sense for your system's stdio ? */
