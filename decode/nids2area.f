      SUBROUTINE MAIN0

C *** ldm-mcidas Revision History ***
C 1 NIDS2AREA.PGM 07-Jan-94,10:00:00,`TCY' Initial release
C *** ldm-mcidas Revision History ***

C ?  NIDS2AREA - Make a McIDAS area from a NIDS plan view product
C ?
C ?  nids2area anum ID <keywords>
C ?
C ?  PARAMETERS:
C ?   anum - McIDAS AREA number (def=8000)
C ?   id   - NEXRAD station ID (def=FTG)
C ?
C ?  KEYWORDS:
C ?   DEV=xxx  - information output control
C ?       ^^^___ debug messages
C ?       ||____ error messages
C ?       |_____ standard informational messages
C ?
C ?   DIALPROD=pcode <day> <time>
C ?            pcode - product code of stream for reference in ROUTE.SYS
C ?            day   - YYDDD of data receipt
C ?            time  - HHMMSS of data receipt
C ?
C ?   nids2area 8001 FTG DIALPROD=R1 X X DEV=CCN < /var/data/nexrad/9501050105.BREF
C ?
C    History: 19990513 - Changed INTEGER*4 declarations to INTEGER
C                        Updated date stored in AREA header(4) to be YYYDDD
C                        Replaced GETDAY/GETTIME with mcgetdaytime/mccydtoyd

      IMPLICIT NONE

      INTEGER	IAND

      INTEGER   AUXSIZ                ! Size of auxiliary block
      INTEGER   DIRSIZ                ! Size of AREA directory
      INTEGER   IAUXOFF               ! Offset to AUX block
      INTEGER   IDATOFF               ! Offset to data
      INTEGER   IDLEN                 ! Size of radar ID in bytes
      INTEGER   IMSIZE                ! Size of image buffer
      INTEGER   INAVOFF               ! Offset to navigation block
      INTEGER   MAGIC                 ! 4-byte word that indicates byte order
      INTEGER   NAMLEN                ! Size of AUX name in bytes
      INTEGER   NAVSIZ                ! Size of navigation block
      INTEGER   UNITLEN               ! Size of AUX data unit in bytes

      PARAMETER (AUXSIZ=64)
      PARAMETER (DIRSIZ=64)
      PARAMETER (IDLEN=8)
      PARAMETER (IMSIZE=100000)
      PARAMETER (MAGIC=Z'04030201')
      PARAMETER (NAMLEN=40)
      PARAMETER (NAVSIZ=128)
      PARAMETER (UNITLEN=8)
      PARAMETER (INAVOFF=        DIRSIZ*4)
      PARAMETER (IAUXOFF=INAVOFF+NAVSIZ*4)
      PARAMETER (IDATOFF=IAUXOFF+AUXSIZ*4)

      CHARACTER*12 CFILE              ! AREA file name
      CHARACTER*12 CPROD              ! File routing product code
      CHARACTER*32 CMEMO              ! AREA memo field
      CHARACTER*12 CNEW               ! New product name (if LW file)
      CHARACTER*4  CNAME              ! Radar Station ID
      CHARACTER*4  CNIDS(14)          ! 4-character product ID
      CHARACTER*11 CMODE(0:2)         ! Radar mode string
      CHARACTER*8  CTILT              ! Radar tilt/bottom-top of layer
      CHARACTER*40 CTITLE             ! Product Title
      CHARACTER*8  CUNIT              ! Data unit

      INTEGER   I                     ! General counter
      INTEGER   IAREA                 ! AREA number
      INTEGER   IBUF(IMSIZE)          ! Image data
      INTEGER   IBYTE                 ! Byte counter
      integer   iday                  ! Day in ccyyddd format
      INTEGER   IHEADER(DIRSIZ)       ! AREA directory
      INTEGER   ILINE(256)            ! One line of image data (ISIZE elements)
      INTEGER   IRTE                  ! Routing table product entry
      INTEGER   ISIGN                 ! Multiplicative sign
      INTEGER   ISTAT                 ! Status
      INTEGER   ITYPE                 ! Type of product 1-based
      INTEGER   IVAL                  ! Integer to hold threshold value
      INTEGER   LINE                  ! Line counter
      INTEGER   NAREA                 ! New AREA number (from routing table)
      INTEGER   NAVARR(NAVSIZ)        ! Navigation array
      INTEGER   NSEC                  ! Number of seconds since 700101

      CHARACTER*12 CKWP               ! Character keyword parameter
      CHARACTER*12 CPP                ! Character positional parameter
      INTEGER   ARANAM                ! Create AREA name given AREA number
      INTEGER   GETRTE                ! Routing
      INTEGER   IPP                   ! Integer positional parameter
      INTEGER   LIT                   ! Convert string to integer literal
      integer   mcgetdaytime          ! Get current day/time in ccyyddd/hhmmss
      integer   mcsectodaytime        ! Convert secs to ccyyddd/hhmmss
      integer   mccydtoiyd            ! Convert ccyyddd to yyyddd
      integer   mccydtoyd             ! Convert ccyyddd to yyddd
      INTEGER   MFD                   ! Convert decimal degrees to DDDMMSS
      INTEGER   OPNARA                ! Open an AREA for writing
      INTEGER   WSIIN                 ! NIDS input routine

      INTEGER*2 MCODE                 ! Message code
      INTEGER*2 IDATE                 ! Days since 700101
      INTEGER   ISEC                  ! Seconds after midnight GMT
      INTEGER   ILEN                  ! Length of block
      INTEGER*2 ISRC                  ! Source
      INTEGER*2 IDEST                 ! Destination
      INTEGER*2 NBLOCK                ! Number of blocks

      COMMON /MSGHEAD/ ISEC, ILEN, MCODE, IDATE, ISRC, IDEST, NBLOCK

      INTEGER*2 ICODE                 ! Product code
      INTEGER*2 IDIVIDE               ! Always -1
      INTEGER   LAT                   ! Radar latitude*1000
      INTEGER   LON                   ! Radar longitude*1000
      INTEGER*2 IHEIGHT               ! Radar elevation [ft]
      INTEGER*2 IMODE                 ! Radar mode 0-> clear air; 1-> precip
      INTEGER*2 ICOVER                ! Coverage
      INTEGER*2 ISEQ                  ! Request sequence number
      INTEGER*2 IVSNUM                ! Volume scan number
      INTEGER*2 IVSDATE               ! Volume scan days since 700101
      INTEGER   IVSTIME               ! Volume scan secs after midnight
      INTEGER*2 IPDAT1                ! Product dependent value
      INTEGER*2 IPDATE                ! Product generation days since 700101
      INTEGER   IPTIME                ! Product generation secs since midnight
      INTEGER*2 IPDAT2                ! Product dependent value
      INTEGER*2 IELNUM                ! Elevation number within volume scan
      INTEGER*2 IPDAT3                ! Product dependent value
      INTEGER*2 ITHRESH(16)           ! Data level thresholds
      INTEGER*2 IPDAT4                ! Product dependent value
      INTEGER*2 IPDAT5                ! Product dependent value
      INTEGER*2 IPDAT6                ! Product dependent value
      INTEGER*2 IPDAT7                ! Product dependent value
      INTEGER*2 IPDAT8                ! Product dependent value
      INTEGER*2 IPDAT9                ! Product dependent value
      INTEGER*2 IPDAT10               ! Product dependent value
      INTEGER*2 NMAPS                 ! # of map pieces if map
      INTEGER   IOFFSYMB              ! # of halfwords to product symbology
      INTEGER   IOFFGRAF              ! # of halfwords to graphic
      INTEGER   IOFFTAB               ! # of halfwords to tabular

      COMMON /PRODDESC/ LAT, LON, IVSTIME, IPTIME, IOFFSYMB, IOFFGRAF,
     +                  IOFFTAB, ICODE, IDIVIDE, IHEIGHT, IMODE,
     +                  ICOVER, ISEQ, IVSNUM, IVSDATE, IPDAT1, IPDATE,
     +                  IPDAT2, IELNUM, IPDAT3, ITHRESH, IPDAT4,
     +                  IPDAT5, IPDAT6, IPDAT7, IPDAT8, IPDAT9,
     +                  IPDAT10, NMAPS

      INTEGER   IPTYPE                ! Type of Product: 0-based index
      INTEGER   NLEVEL                ! Number of data levels
      INTEGER*2 IELEV                 ! Elevation [0.1 degrees] or bottom
                                      !   of layer [ft]
      INTEGER*2 ITOP                  ! Top of layer [ft]
      REAL*4    VMIN                  ! Minimum data value
      REAL*4    VMAX                  ! Maximum data value
      INTEGER   IPSTART               ! Precip start time
      INTEGER   IPEND                 ! Precip end time
      INTEGER   ISIZE                 ! Size of raster image
      REAL*4    DATRES                ! Data resolution [km]
      REAL*4    RASRES                ! Raster resolution [km]
     
      COMMON /PRODINFO/ VMIN, VMAX, DATRES, RASRES, IPTYPE, NLEVEL,
     +                  IPSTART, IPEND, ISIZE, IELEV, ITOP

C..... Routing table entries
      INTEGER RSTAT         ! Routing status 0=SUSpended , 1=RELeased
      INTEGER RPROD         ! Product name
      INTEGER RTYPE         ! Product type (AREA,MD,GRID,TEXT)
      INTEGER RBEG          ! Cylinder beginning cylinder
      INTEGER REND          ! Cylinder ending cylinder
      INTEGER RNOW          ! Current product cylinder
      INTEGER RCDAY         ! Creation day [YYDDD]
      INTEGER RCTIM         ! Creation time [HHMMSS]
      INTEGER RRDAY         ! Receive day [YYDDD]
      INTEGER RRTIM         ! Receive time [HHMMSS]
      INTEGER RMEMO         ! Memo filed (32 characters)
      INTEGER RPOST         ! PostProcess BATCH file name
      INTEGER RMASK         ! Text file name mask for type=TEXT
      INTEGER RTEXT         ! Last text file name created
      INTEGER RCOND         ! Decoder condition code (1-7, def=1)
      INTEGER RLAST         ! Last successful product number
      INTEGER RLTXT         ! Last successful text file name
      INTEGER RXCUT         ! Execute PostProcess BATCH file based on RCOND
      INTEGER RSYSB         ! >0 -> SYSKEY index for beginning cylinder number
      INTEGER RSYSE         ! >0 -> SYSKEY index for ending cylinder number
      INTEGER RSYSC         ! >0 -> SYSKEY index for current cylinder number
      INTEGER RDUM          ! RESERVED

      COMMON/RTECOM/ RSTAT,RPROD,RTYPE,RBEG,REND,RNOW,RCDAY,RCTIM,
     *               RRDAY,RRTIM,RMEMO(8),RPOST(3),RMASK(3),RTEXT(3),
     *               RCOND,RLAST,RLTXT(3),RXCUT,RSYSB,RSYSE,RSYSC,
     *               RDUM(28)

      EQUIVALENCE (RNOW,IAREA)

      INTEGER   AUXBLK(AUXSIZ)  ! NIDS auxiliary information
                                ! word(s)    meaning
                                ! 1          MAGIC (Z'04030201')
                                ! 2          AUXSIZ #bytes in entry
                                ! 3          #bytes in entry name
                                ! 4          #bytes in entry
                                ! 5          entry name; fixed as 'INFO'
                                ! 6-15       title
                                ! 16         product code
                                ! 17         length of radar ID in bytes
                                ! 18-19      radar site ID [up to 8 chars]
                                ! 20         radar latitude
                                ! 21         radar longitude
                                ! 22         radar elevation
                                ! 23         volume scan DAY [CCYYDDD]
                                ! 24         volume scan TIME [HHMMSS]
                                ! 25         radar mode [0|1|2] clear air,
                                !              precipitation, storm
                                ! 26         range [km]
                                ! 27         resolution [km]
                                ! 28         update rate [10|6|5]
                                ! 29-30      tilt [deg] | layer [8 chars]
                                ! 31         min/max scale factor
                                ! 32         min data value
                                ! 33         max data value
                                ! 34         start DAY [YYDDD]
                                ! 35         start TIME [HHMMSS]
                                ! 36         end DAY [YYDDD]
                                ! 37         end TIME [HHMMSS]
                                ! 38         UNITLEN #bytes in unit
                                ! 39-40      data unit [8 characters]
                                ! 41         # data levels
                                ! 42         data offset
                                ! 43         data scale
                                ! 44-AUXSIZ  calibrated data value

      DATA CNIDS /'????', 'BREF', 'VEL ', 'CREF', 'LAVE', 'LREF', 
     +            'TOPS', 'VIL ', 'PRE1', 'PRE3', 'PRET', 'HDRA',
     +            'BRLR', 'SRMV' /

      DATA CMODE /'Maintenance', 'Clear Air  ', 'Precip Mode'/

C
C..... Begin
C
      CALL SDEST('NIDS2AREA -- BEGIN',0)

C..... Get Positional parameter(s)
      IAREA = IPP(1,8000)

      CNAME = CPP(2,'FTG')

C..... Check for file routing; exit if product receipt is suspended
      CPROD = CKWP('DIALPROD',1,' ')
      IF (CPROD .NE. ' ') THEN
        IRTE = GETRTE (IAREA,NAREA,CNEW)
        IF (IRTE .LT. 0) THEN
          CALL EDEST ('Product ROUTEing SUSpended',0)
          GO TO 900
        ENDIF
        IAREA = NAREA
      ENDIF
      ISTAT = ARANAM(IAREA,CFILE)

C..... Get the NIDS product
      ISTAT = WSIIN (IBUF, 4*IMSIZE, CNAME)
      IF (ISTAT .NE. 0) THEN
        CALL EDEST('ERROR reading NIDS product.  Aborting!',ISTAT)
        GOTO 900
      ENDIF
      CALL DDEST('Product size: ',ISIZE)
      CALL DDEST('Radar ID: '//CNAME,0)
      CALL MOVCW(CFILE,RTEXT)         ! Save file name in routing table

C..... Fill the AREA directory block
      CALL ZEROW(DIRSIZ,IHEADER)      ! Zero AREA directory buffer
      IHEADER(2) = 4                  ! Word 4 of valid AREAs is always 4
      IHEADER(3) = 7                  ! Sensor Source

      NSEC = 86400 * (IVSDATE-1) + IVSTIME  ! put date into Julian format
c     CALL SKHMS (NSEC-63072000,IHEADER(4),IHEADER(5))
      istat = mcsectodaytime( nsec, iday, iheader(5) ) ! iday -> [CCYYDDD]
      istat = mccydtoiyd ( iday, iheader(4) ) ! iheader(4) -> date [YYYDDD]

      IHEADER(6)  = 1                 ! upper left line of AREA
      IHEADER(7)  = 1                 ! upper left element of AREA
      IHEADER(9)  = ISIZE             ! # lines
      IHEADER(10) = ISIZE             ! # elements (NIDS AREAs always square)
      IF (MOD(ISIZE,4) .NE. 0) THEN   ! #ele/4 must be even
        IHEADER(10) = 4 * (ISIZE/4 + 1)
      ENDIF
      IHEADER(11) = 1                 ! # bytes/element
      IHEADER(12) = 1                 ! line resolution
      IHEADER(13) = 1                 ! element resolution
      IHEADER(14) = 1                 ! number of bands/line

      IHEADER(16) = 9999              ! project number
c     CALL GETDAY(IHEADER(17))        ! AREA creation date
c     CALL GETTIM(IHEADER(18))        ! AREA creation date
      istat = mcgetdaytime( iday, iheader(18) )
      istat = mccydtoyd( iday, iheader(17) )
      IHEADER(21) = LIT(CNAME)        ! Station ID (4 characters)
      IHEADER(22) = ICODE             ! Product code
      IHEADER(23) = IMODE             ! Radar operation mode
      IHEADER(24) = NLEVEL            ! Number of data levels

C..... AUX block common information
      AUXBLK( 1) = MAGIC              ! byte swap check value
      AUXBLK( 2) = AUXSIZ*4           ! total size of AUX block in bytes
      AUXBLK( 3) = 4                  ! length of entry name in bytes
      AUXBLK( 4) = (AUXSIZ-5)*4       ! length of entry bytes
      AUXBLK( 5) = LIT('INFO')        ! AUX block name fixed as 'INFO'

      AUXBLK(16) = ICODE              ! product code
      AUXBLK(17) = IDLEN              ! length of radar ID in bytes
      AUXBLK(18) = IHEADER(21)        ! radar ID
      AUXBLK(19) = LIT('    ')        ! 2nd 4 bytes of radar ID
      AUXBLK(20) = MFD(LAT*1.D-3)     ! radar latitude [DDDMMSS]
      AUXBLK(21) = MFD(-LON*1.D-3)    ! radar longitude [DDDMMSS]
      AUXBLK(22) = IHEIGHT            ! radar elevation
      AUXBLK(23) = iday               ! volume scan DAY [CCYYDDD]
      AUXBLK(24) = IHEADER(5)         ! volume scan TIME [HHMMSS]
      AUXBLK(25) = IMODE              ! radar operation mode
      AUXBLK(26) = ISIZE*DATRES       ! range [km]
      AUXBLK(27) = DATRES             ! data resolution [km]
      IF (IMODE .EQ. 0) THEN          ! update rate [10|6|5] [min]
        AUXBLK(28) = 10               ! clear air mode; updates every 10 min
      ELSE IF (IMODE .EQ. 1) THEN
        AUXBLK(28) = 6                ! precip mode; updates every 6 min
      ELSE
        AUXBLK(28) = 5                ! storm mode; updates every 5 min
      ENDIF

      AUXBLK(34) = AUXBLK(23)         ! start DAY
      AUXBLK(35) = AUXBLK(24)         ! start TIME
      AUXBLK(36) = AUXBLK(23)         ! start DAY
      AUXBLK(37) = AUXBLK(24)         ! start TIME
      AUXBLK(38) = UNITLEN            ! unit length in bytes

      AUXBLK(41) = NLEVEL             ! number of data levels
      AUXBLK(42) = 0                  ! data offset value

C..... Get data scaling and offset
      IVAL   = ITHRESH(1)
      AUXBLK(43) = 1                  ! assume no scaling
      IF (IAND(IVAL,Z'00001000').NE.0) AUXBLK(43) = 10
      IF (IAND(IVAL,Z'00002000').NE.0) AUXBLK(43) = 20

      AUXBLK(31) = AUXBLK(43)*10      ! min/max scale factor
      AUXBLK(32) = VMIN*10            ! scaled minimum data value
      AUXBLK(33) = VMAX*10            ! scaled minimum data value

      DO 100 I = 1, NLEVEL            ! data threshold values
        IVAL  = ITHRESH(I)
        IF (IAND(IVAL,Z'00008000') .EQ. 0) THEN
          ISIGN = -1
          IF (IAND(IVAL,Z'00000100') .EQ. 0) ISIGN = 1
          AUXBLK(43+I) = ISIGN * IAND(IVAL,Z'000000FF')
        ELSE                          ! 0->BLANK; 1->TH; 2->ND; 3->RF
          AUXBLK(43+I) = -9999 + IAND(IVAL,Z'000000FF')
        ENDIF
100   CONTINUE

C..... Create memo field based on product type
      CMEMO = ' '
      ITYPE = IPTYPE + 1

      IF      (CNIDS(ITYPE) .EQ. 'BREF') THEN     ! base reflectivity
        WRITE (CMEMO,'("Base Reflct ",F4.2," DEG ", A)') 
     +         IELEV/10., CMODE(IMODE)
        IHEADER(19) = 1                           ! Filter map (BAND)
        CTILT  = CMEMO(13:20)
        CTITLE = 'BREF: Base Reflectivity'
        CUNIT  = 'DBZ'

      ELSE IF (CNIDS(ITYPE) .EQ. 'BRLR') THEN     ! 248 nm base reflectivity
        WRITE (CMEMO,'("BaseRef 248 ",F4.2," DEG ", A)') 
     +         IELEV/10., CMODE(IMODE)
        IHEADER(19) = 1                           ! Filter map (BAND)
        CTILT  = CMEMO(13:20)
        CTITLE = 'BREF: 248 nm Base Reflectivity'
        CUNIT  = 'DBZ'

      ELSE IF (CNIDS(ITYPE) .EQ. 'CREF') THEN     ! composite reflectivity
        WRITE (CMEMO,'("Composite Reflect ", A)') CMODE(IMODE)
        IHEADER(19) = 2                           ! Filter map (BAND)
        CTILT  = '-99'
        CTITLE = 'CREF Composite Reflectivity'
        CUNIT  = 'DBZ'

      ELSE IF (CNIDS(ITYPE) .EQ. 'LAVE') THEN     ! layer average reflectivity
        CALL EDEST('Product type not supported: '//CNIDS(ITYPE),0)
        RETURN

      ELSE IF (CNIDS(ITYPE) .EQ. 'LREF') THEN     ! layer maximum reflectivity
        WRITE (CMEMO,'("Layer Reflct ",I2,"-",I2," ",A)') 
     +         IELEV, ITOP, CMODE(IMODE)
        IHEADER(19) = 4                           ! Filter map (BAND)
        CTILT  = CMEMO(14:19)
        CTITLE = 'LREF: Layer Composite Reflectivity'
        CUNIT  = 'DBZ'

      ELSE IF (CNIDS(ITYPE) .EQ. 'TOPS') THEN     ! echo tops
        WRITE (CMEMO,'("Echo Tops [K FT] ", A)') CMODE(IMODE)
        IHEADER(19) = 8                           ! Filter map (BAND)
        CTILT  = '-99'
        CTITLE = 'TOPS: Echo Tops'
        CUNIT  = 'K FT'

      ELSE IF (CNIDS(ITYPE) .EQ. 'PRE1') THEN     ! 1-hour precip total
        WRITE (CMEMO,'("1-hr Rainfall [IN] ", A)') CMODE(IMODE)
        IHEADER(19) = 16                          ! Filter map (BAND)
        CTILT  = '-99'
        CTITLE = 'PRE1: Surface 1-hour Rainfall Total'
        CUNIT  = 'IN'

      ELSE IF (CNIDS(ITYPE) .EQ. 'PRE3') THEN     ! 3-hour precip total
        WRITE (CMEMO,'("3-hr Rainfall [IN] ", A)') CMODE(IMODE)
        IHEADER(19) = 32                          ! Filter map (BAND)
        CTILT  = '-99'
        CTITLE = 'PRE3: Surface 3-hour Rainfall Total'
        CUNIT  = 'IN'

      ELSE IF (CNIDS(ITYPE) .EQ. 'PRET') THEN     ! storm total precip
        WRITE (CMEMO,'("Strm Tot Rain [IN] ", A)') CMODE(IMODE)
        IHEADER(19) = 64                          ! Filter map (BAND)
c       CALL SKHMS (IPSTART-63072000,AUXBLK(34),AUXBLK(35)) !precip start TIME
        istat = mcsectodaytime( ipstart, iday, AUXBLK(35) )
        istat = mccydtoyd ( iday, auxblk(34) )
c       CALL SKHMS (IPEND  -63072000,AUXBLK(36),AUXBLK(37)) !precip end TIME
        istat = mcsectodaytime( ipend, iday, auxblk(37) )
        istat = mccydtoyd ( iday, auxblk(36) )
        CTILT  = '-99'
        CTITLE = 'PRET: Surface Storm Total Rainfall'
        CUNIT  = 'IN'

      ELSE IF (CNIDS(ITYPE) .EQ. 'VIL ') THEN     ! vertically integ. H2O
        WRITE (CMEMO,'("Vert Int Lq H2O [mm] ", A)') CMODE(IMODE)
        IHEADER(19) = 128                         ! Filter map (BAND)
        CTILT  = '-99'
        CTITLE = 'VIL: Vertically-integrated Liquid Water'
        CUNIT  = 'kg/m^2'

      ELSE IF (CNIDS(ITYPE) .EQ. 'VEL ') THEN     ! radial velocity
        WRITE (CMEMO,'("Rad Vel ",F4.2," DEG ", A)') 
     +        IELEV/10., CMODE(IMODE)
        IHEADER(19) = 256                         ! Filter map (BAND)
        CTILT  = CMEMO(9:16)
        CTITLE = 'VEL: Radial Velocity'
        CUNIT  = 'KT'

      ELSE IF (CNIDS(ITYPE) .EQ. 'SRMV') THEN     ! radial velocity
        WRITE (CMEMO,'("StrmRelMnVl ",F4.2," DEG ", A)') 
     +        IELEV/10., CMODE(IMODE)
        IHEADER(19) = 256                         ! Filter map (BAND)
        CTILT  = CMEMO(9:16)
        CTITLE = 'SRMV: Storm Relative Mean Velocity'
        CUNIT  = 'KT'

      ELSE IF (CNIDS(ITYPE) .EQ. 'HDRA') THEN     ! hourly digital rainfall
        CALL EDEST('Product type not image: '//CNIDS(ITYPE),0)
        RETURN

      ELSE                                        ! product unrecognized
        CALL EDEST('Product type unknown: '//CNIDS(ITYPE),0)
        RETURN

      ENDIF

      CALL MOVCW(CMEMO,IHEADER(25))

      IHEADER(33) = IAREA             ! AREA number
      IHEADER(34) = IDATOFF           ! byte offset to beginning of data
      IHEADER(35) = INAVOFF           ! byte offset to beginning of navigation

      IHEADER(46) = IHEADER(4)        ! actual image start date
      IHEADER(47) = IHEADER(5)        ! actual image start time

      IHEADER(52) = LIT('NIDS')       ! image source type
      IHEADER(53) = LIT('RAW ')       ! calibration type

      IHEADER(60) = IAUXOFF           ! byte offset to beginning of aux block #1
      IHEADER(61) = 1                 ! number of AUX blocks in file
      IHEADER(63) = IAUXOFF           ! byte offset to beginning of calibration

C..... Create the AREA writing the directory
      CALL MAKARA(IAREA,IHEADER)

C..... Fill in the navigation block
      CALL MOVCW(CTILT,AUXBLK(29))                ! tilt or layer top & bottom
      CALL MOVCW(CTITLE(1:NAMLEN),AUXBLK(6))      ! product title
      CALL MOVCW(CUNIT(1:UNITLEN),AUXBLK(39))     ! unit

      NAVARR(1) = LIT('RADR')         ! navigation type
      NAVARR(2) = ISIZE / 2           ! row (image coord) of radar site
      NAVARR(3) = ISIZE / 2           ! col ((image coord) of radar site)
      NAVARR(4) = AUXBLK(20)          ! format latitude into DDDMMSS form
      NAVARR(5) = AUXBLK(21)          ! McIDAS longitude is opposite NIDS
      NAVARR(6) = RASRES*1000         ! pixel resolution [m]
      NAVARR(7) = 0                   ! rotation of north from vertical
      NAVARR(8) = 0                   ! pixel resolution [m] in long direction

C..... Write the navigation block to the AREA
c     CALL MAKARA(IAREA,IHEADER)
      ISTAT = OPNARA(IAREA)
      CALL ARAPUT(IAREA,INAVOFF,NAVSIZ*4,NAVARR)

C..... Write the AUX block(s)
      CALL ARAPUT(IAREA,IAUXOFF,AUXSIZ*4,AUXBLK)

C..... Write the AREA.  AREA scans are from top-to-bottom; NIDS are from
C      bottom-to-top
      CALL ZEROW(256,ILINE)
      IBYTE = ISIZE * (ISIZE - 1)     ! first byte of last NIDS row
      DO 500 LINE = 0, ISIZE-1
        CALL MOVC(ISIZE,IBUF,IBYTE,ILINE,0)
        IBYTE = IBYTE - ISIZE
        CALL WRTARA(IAREA,LINE,ILINE)
500   CONTINUE

C..... Close the AREA
      CALL CLSARA(IAREA)

C..... Update ROUTE.SYS if needed <<<<< UPC mod 960611 >>>>>
      IF (IRTE .GT. 0) THEN
        CALL FSHRTE(IRTE,1)
      ENDIF

C..... Done
      CALL SDEST('NIDS2AREA -- DONE AREA ',IAREA)
900   RETURN
      END


C     INTEGER FUNCTION GETRTE(INUM,ONUM,CTEXT)
C
C     NOTE: special version for nids2area
C
C     GETRTE: get routing code entry for specified code
C           INUM (I) - default SSEC product destination #
C           ONUM (I) - new product destination #
C           CTEXT (C) - new product name (if LW file)
C           func value- =0 = NO TABLE -- file product using defaults
C                       >0 = file product using values in RTECOM
C                       <0 = do not file product
C
C     History: 19990518 - write new AREA number back to ROUTE.SYS immediately
C                         so concurrent nids2area invocation won't get same
C                         output AREA file number
C

      FUNCTION GETRTE(INUM,ONUM,CTEXT)
      IMPLICIT INTEGER(A-B,D-Z)
      IMPLICIT CHARACTER*12 (C)

      CHARACTER*2 CNAMES
      CHARACTER*4 CLIT

      PARAMETER (ENTWRD=64)
      PARAMETER (OFFSET=128)
      PARAMETER (MAXNAM=256)

      COMMON/RTECOM/ RSTAT,RPROD,RTYPE,RBEG,REND,rnow,RCDAY,RCTIM,
     &               RRDAY,RRTIM,RMEMO(8),RPOST(3),RMASK(3),RTEXT(3),
     &               rcond,rlast,rltxt(3),rxcut,rsysb,rsyse,rsysc,
     &               RDUM(28)

C     WORD(S)  NAME    DESCRIPTION
C       1      RSTAT   ROUTING STATUS 0=SUSpended , 1=RELeased
C       2      RPROD   PROUDUCT NAME
C       3      RTYPE   PROUDUCT TYPE (AREA,MD,GRID,TEXT)
C       4      RBEG    CYLINDER BEGINING NUMBER
C       5      REND    CYLINDER ENDING NUMBER
C       6      rnow    Current product number filed in cylinder
C       7      RCDAY   CREATE DAY (YYDDD)
C       8      RCTIM   CREATE TIME (HHMMSS)
C       9      RRDAY   RECEIVE DAY (YYDDD)
C      10      RRTIM   RECEIVE TIME (HHMMSS)
C     11-18    RMEMO   MEMO FIELD (32 CHARACTER)
C     19-21    RPOST   BATCH FILE NAME FOR POST PROCESS
C     22-24    RMASK   TEXT FILE NAME MASK FOR TYPE=TEXT
C     25-27    RTEXT   LAST TEXT FILE NAME CREATED
C      28      rcond   Decoder condition code (1-7 , def=1)
C      29      rlast   Last successful product number (defined by rcond)
C     30-32    rltxt   Last successful text file name (defined by rcond)
C      33      rxcut   Execute Post Process batch file based on rcond
C      34      rsysb   if >0 -- SYSKEY index for beginning cylinder number
C      35      rsyse   if >0 -- SYSKEY index for ending cylinder number
C      36      rsysc   if >0 -- SYSKEY index for current cylinder number
C     37-64    RDUM    RESERVED

      DIMENSION CNAMES(MAXNAM),NAMES(OFFSET)

      EQUIVALENCE (CNAMES,NAMES)

      DATA CFILE/'ROUTE.SYS'/

C--- SET INITIAL RETURN CODE -- TAKE DEFAULT
      GETRTE=0
      ONUM=INUM
      CNAME=' '

C--- CHECK EXISTANCE OF ROUTING TABLE
      IF(LWFILE(CFILE).EQ.0) RETURN

C--- GET THE PRODUCT/CREATE DATE/CREATE TIME FROM KEYIN
      CPROD=CKWP('DIALPROD',1,' ')

C--- LOCK THE FILE
      CALL LOCK(CFILE)

C--- GET NAME TABLE
      VAL = LWI(CFILE,0,OFFSET,NAMES)

C--- SCAN TABLE FOR MATCHING PRODUCT ID
      DO 10 I=1,MAXNAM
      IF(CPROD(1:2).EQ.CNAMES(I)(1:2)) THEN

            GETRTE = -1

C--- DETERMINE THE POINTER TO THE PRODUCT ENTRY AND READ THE ENTRY
            PTR = (I-1)*ENTWRD+OFFSET
            VAL = LWI(CFILE,PTR,ENTWRD,RSTAT)
            CALL FLIPRTE                         ! <<<<< UPC add 961029 >>>>>

            IF(RSTAT.NE.0) THEN

C--- PRODUCT IS RELEASED (ACTIVE)
               GETRTE=I

C--- PRODUCT CREATE DAY AND TIME
               RCDAY=IKWP('DIALPROD',2,0)
               RCTIM=IKWP('DIALPROD',3,0)
               CDAY=CFU(RCDAY)
               CTIM=CFJ(RCTIM)
               CALL SDEST('PRODUCT CODE='//CPROD//CDAY//CTIM(7:12),0)

C--- PRODUCT RECEIVE DAY AND TIME
               istat = mcgetdaytime(rrday, rrtim) ! <<<<< UPC mod 981207 >>>>>
c              CALL GETDAY(RRDAY)
c              CALL GETTIM(RRTIM)

               IF(RBEG.EQ.-1.AND.REND.EQ.-1) THEN
C--- USE THE DEFAULT

                 rnow = INUM
                 ONUM = INUM
                 CALL MOVWC(rmask,cmask)
                 if(cmask(1:1) .ne. ' ') ctext = cmask
                 call movcw(ctext,rtext)

               ELSE

C--- CYLINDER IS DEFINED: COMPUTE THE NEW NUMBER
                 IF(rnow.EQ.-1) THEN
                    rnow=RBEG
                 ELSE
                    rnow=rnow+1
                    IF(rnow.GT.REND) rnow=RBEG
                 ENDIF

                 ONUM = rnow

                 call swbyt4(rnow,1)             ! <<<<< UPC mod 19990518 >>>>>
                 istat = lwo(cfile,ptr+5,1,rnow) ! <<<<< UPC mod 19990518 >>>>>
                 rnow = ONUM                     ! <<<<< UPC mod 19990518 >>>>>

                 CTYPE=CLIT(RTYPE)
                 IF(CTYPE(1:4).EQ.'TEXT') THEN
                    CALL MOVWC(RMASK,CMASK)
                    if(cmask(1:1) .eq. ' ') cmask = ctext
                    LEN=NCHARS(CMASK,IB,IE)
                    CNUM=CFJ(ONUM)
                    CTEXT=CMASK(IB:IE)//'.'//CNUM(10:12)
                    CALL MOVCW(CTEXT,RTEXT)
                 ENDIF

               ENDIF
            ENDIF

            GOTO 999
      ENDIF
10    CONTINUE

C--- UNLOCK THE FILE
999   CALL UNLOCK(CFILE)

      RETURN
      END


C     SUBROUTINE FSHRTE(IRTE, ISTAT)
C
C     NOTE: special version for nids2area
C
C     FSHRTE: Flush the routing packet back to routing table (RCD)
C           IRTE (I) - input  ptr returned by GETRTE
C           ISTAT (I) - input  decoder status
C                              -2 == total failure
C                              -1 == partial success
C                               1 == total success

      SUBROUTINE FSHRTE(IRTE, ISTAT)
      IMPLICIT INTEGER(A-B,D-Z)
      IMPLICIT CHARACTER*12 (C)

      PARAMETER (OFFSET=128)
      PARAMETER (ENTWRD=64)

      CHARACTER*160 CKYN
      CHARACTER*4 CPROD,CLIT

      COMMON/RTECOM/ RSTAT,RPROD,RTYPE,RBEG,REND,rnow,RCDAY,RCTIM,
     &               RRDAY,RRTIM,RMEMO(8),RPOST(3),RMASK(3),RTEXT(3),
     &               rcond,rlast,rltxt(3),rxcut,rsysb,rsyse,rsysc,
     &               RDUM(28)

      DATA CFILE/'ROUTE.SYS'/

      CALL MOVWC(RPOST,CPOST)
      CALL MOVWC(RPROD,CPROD)
      call movwc(rtext,ctext)
      call movwc(rltxt,cltxt)

      PTR=(IRTE-1)*ENTWRD+OFFSET

c--- check condition code
      pass = 0
      if(istat .eq. 1) then
       if(rcond.eq.0.or.rcond.eq.1.or.rcond.eq.3.or.rcond.eq.5.or.
     &    rcond.eq.7) pass=1
      else if(istat .eq. -1) then
       if(rcond.eq.2.or.rcond.eq.3.or.rcond.eq.6.or.rcond.eq.7) pass=1
      else
       if(rcond.eq.4.or.rcond.eq.6.or.rcond.eq.7) pass=1
      endif

c--- pass or fail
      if(pass .eq. 0) then
         call sdest('Product Routing: Product failed condition test=',
     &               rcond)
         rnow = rlast
         call movcw(cltxt,rtext)
         CALL FLIPRTE                            ! <<<<< UPC add 961029 >>>>>
         VAL=lwo(CFILE,PTR,5,RSTAT)              ! <<<<< UPC mod 990519 >>>>>
         VAL=lwo(CFILE,PTR+6,ENTWRD-6,RCDAY)     ! <<<<< UPC mod 990519 >>>>>
         CALL FLIPRTE                            ! <<<<< UPC add 961029 >>>>>
         if(rxcut .eq. 0) return
      else
         rlast = rnow
         call movcw(ctext,rltxt)
         CALL FLIPRTE                            ! <<<<< UPC add 961029 >>>>>
         VAL=lwo(CFILE,PTR,5,RSTAT)              ! <<<<< UPC mod 990519 >>>>>
         VAL=lwo(CFILE,PTR+6,ENTWRD-6,RCDAY)     ! <<<<< UPC mod 990519 >>>>>
         CALL FLIPRTE                            ! <<<<< UPC add 961029 >>>>>
         if( rsysc .ne. 0 ) then                 ! <<<<< UPC mod 940818 >>>>>
           IF      (RTYPE .EQ. LIT('TEXT')) THEN
              call sysin(rsysb,rbeg)
              call sysin(rsyse,rend)
              call sysin(rsysc,rlast)
           ELSE IF (RTYPE .EQ. LIT('AREA')) THEN
              call sysin(rsysb,rbeg)
              call sysin(rsyse,rend)
              call sysin(rsysc,rnow)
           ELSE IF (RTYPE .EQ. LIT('GRID')) THEN
              call sysin(rsysb,rlast)
              call sysin(rsyse,rctim/10000)           ! Time [HH]
              call sysin(rsysc,rcday)
           ELSE IF (RTYPE .EQ. LIT('MD  ')) THEN
              call sysin(rsysb,rlast)
              call sysin(rsyse,rctim/10000)           ! Time [HH]
              call sysin(rsysc,rcday)
           ENDIF
         endif
      endif

      IF(CPOST.NE.' ') THEN
        LEN=NCHARS(CPOST,IB,IE)
        CTYPE=CLIT(RTYPE)
        IF(CTYPE.EQ.'TEXT') THEN
          cval = ctext
        ELSE
          CVAL=CFU(rnow)
        ENDIF
        LEN=NCHARS(CVAL,JB,JE)
        CDAY=CFJ(RCDAY)
        CTIM=CFJ(RCTIM)
        code = cfu(istat)
C       <<<<< UPC mod 950319 - Added TWIN=0 to BATCH command >>>>>
        CKYN='BATCH '//CPROD(1:2)//' '//CVAL(JB:JE)//' '//CDAY(8:12)//
     &       ' '//CTIM(7:12)//' '//code(1:2)//' TWIN=0 "'//CPOST(IB:IE)
        CALL KEYIN(CKYN)
      ENDIF

      RETURN
      END

