# LDM-MCIDAS
The LDM-McIDAS Decoders set of LDM-compatible decoders for products contained in the Unidata-Wisconsin (LDM feed type UNIWISC aka MCIDAS), NIMAGE FSL2 and LIGHTNING data streams that are available by IDD feed to core Unidata sites.


INTRODUCTION:

This package contains LDM decoders for generating McIDAS products from
the McIDAS, NIDS, and NLDN data streams.



PACKAGE COMPONENTS:

    Directory		Contents
    ---------		--------

    decode/		Sources for LDM decoders for McIDAS data:
			nldn2md(1), proftomd(1), pnga2area(1), pngg2gini(1),
			zlibg2gini(1).

			Obsolete decoders no longer built by default:
			cdftomd(1), lwtoa3(1), gunrv2(1), lwfile(1), lwtmd2(1), 
			and nids2area(1).

    port		Portability stuff and miscellaneous support
			functions.

    libpng		PNG compression library stuff.neous support

    zlib		zlib compression library stuff.neous support


************************************************************************
INSTALLATION:

Please read the file INSTALL and follow its instructions.

************************************************************************
USAGE:

Manual pages for each utility should become available when the package
is installed.  You might, however, have to adjust your MANPATH environment
variable in order to access them: e.g.

    % make install
    ...
    % setenv MANPATH $MANPATH:/usr/local/ldm/ldm-mcidas/man
    % man 1 pnga2area
    ...
    % man 1 lwtoa3
    ...

Also, if you intend to use the decode utilities as LDM decoders, then you
should be familiar with the chapter on LDM configuration in the LDM Site
Manager's Guide.

************************************************************************
NOTES:

Several decoders still included in this distribution are considered
obsolete: cdftomd(1), lwtoa3(1), gunrv2(1), lwfile(1), lwtmd2(1),
and nids2area(1).  Even though source code is still included for
these decoders, executables are not created by the default 'make'
invocation.  If you still need these decoders, please contact Unidata
User Support <support@unidata.ucar.edu> for assistance.

area2png(1) is used to compress McIDAS AREA files.  The AREA file headers
(directory, navigation, calibration, aux) and audit block (located after
the image portion of the file) are stored uncompressed at the beginning
of the PNG compressed file.  This way, the directory information can be
read, and utilities that can display PNG compressed images can display
be used to display the compressed AREA file image (after skipping the
header information).

pnga2area(1) is used to put area2png(1) compressed AREA file imagery
back into AREA file format.

pngg2gini(1) is used to convert PNG-compressed GINI format imagery
into uncompressed GINI format imagery.  The PNG-compressed images are
no longer available in the Unidata IDD NIMAGE datastream, so it is
unlikely that you will use this decoder.

zlibg2gini(1) is used to convert the ZLIB-compressed GINI imagery in
the NWS NOAAPORT broadcast to uncompressed GINI images.  Unidata-supported
display and analysis applications (GEMPAK, the IDV, and McIDAS) can
use the ZLIB-compressed images directly, so you will probably not need
this decoder.  It could, however, be useful if you use some other
display/analysis package that requires that the images be uncompressed
before being read.

************************************************************************
FEEDBACK:

Please send questions, bug reports, complaints, praise, criticisms, etc. to
the following address at the Unidata Program Center:

support@unidata.ucar.edu
