      SUBROUTINE MAIN0
C ?  NLDN2MD - WRITE NLDN DATA TO AN MD FILE
C ?
C ?  NLDN2MD mdf schema keywords
C ?
C ?  PARAMETERS
C ?   mdf    - base number of output MD file 
C ?            o data will be appended to exiting file
C ?            o file number overridden by entry in ROUTE.SYS
C ?
C ?   schema - name of registered schema to apply to data (input)
C ?
C ?  KEYWORDS
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
C ?  EXAMPLE
C ?   NLDN2MD 70 NLDN DIALPROD=LD X X DEV=CCN
C ?
C ?  HISTORY: 19981203 - multiple modifications for Y2K compatibility
C ?                        (dates in form YYDDD -> CCYYDDD)
C ?

      IMPLICIT NONE

      INTEGER*4 HEX80, MAXTOK, MAXSIZ
c     PARAMETER (HEX80='80808080'X)
      PARAMETER (HEX80=-2139062144)
      PARAMETER (MAXTOK = 64)
      PARAMETER (MAXSIZ = 400)

      CHARACTER*4 CLIT
      CHARACTER*12 CKWP, CPP, CFILE, CPROD, CNEW, CSCHE
      CHARACTER*32 CTITLE               ! <<<<< UPC add 960531 >>>>>

      INTEGER SCALES(MAXSIZ), UNITS(MAXSIZ), LOCS(MAXSIZ)
      INTEGER RECORD(MAXSIZ)
      INTEGER IBUF(MAXTOK), IHEDR(MAXTOK), LIST(MAXTOK)

      INTEGER ICOL, IROW, JCOL, JROW, NREC
      INTEGER IEOF, INEW, IRTE, ISTAT, MDBASE, MDF
      INTEGER NCPOS, NDPOS, NPOS, NTOK
      INTEGER NC, NR, NCKEYS, NDKEYS, NKEYS, NRKEYS
      INTEGER ISCHE, IVSN, NDOFF
      INTEGER ID, MREC, NFLSH
      INTEGER IDAY, ITIME, JDAY, JDATE, JTIME
      INTEGER LWO, MDI, MDINFO, MDKEYS, MDMAKE, MDOIMP
      INTEGER GETRTE, NLDNIN
      INTEGER IPP, LIT, MOD, IDNINT
      INTEGER TITLE(8)                   ! <<<<< UPC add 960531 >>>>>
      INTEGER mccydtostr                 ! <<<<< UPC add 981203 >>>>>
      INTEGER m0mdtimeput                ! <<<<< UPC add 970909 >>>>>
      INTEGER mcsectodaytime             ! <<<<< UPC add 981203 >>>>>

      INTEGER ISEC, MULT, NANO
      REAL*4  ANG, ECCEN, SEMI, SGNL, XCHSQ, XLAT, XLON

      INTEGER RSTAT,RPROD,RTYPE,RBEG,REND,rnow,RCDAY,RCTIM,
     *        RRDAY,RRTIM,RMEMO,RPOST,RMASK,RTEXT,
     *        rcond,rlast,rltxt,rxcut,rsysb,rsyse,rsysc,
     *        RDUM

      COMMON/RTECOM/ RSTAT,RPROD,RTYPE,RBEG,REND,rnow,RCDAY,RCTIM,
     *               RRDAY,RRTIM,RMEMO(8),RPOST(3),RMASK(3),RTEXT(3),
     *               rcond,rlast,rltxt(3),rxcut,rsysb,rsyse,rsysc,
     *               RDUM(28)

      EQUIVALENCE (RNOW,MDF), (CTITLE,TITLE)   ! <<<<< UPC mod 960531 >>>>>

      DATA  CTITLE /"NLDN data for                   "/
C
C..... Begin
C
      CALL SDEST('NLDN2MD -- BEGIN',0)
      NREC = 0

C
C..... Retrieve positional parameters
C

C..... MD file number base
      MDF = IPP(1,70)
      IF (MDF .LE. 0) THEN
        CALL EDEST('INVALID MD FILE NUMBER: ',MDF)
        GO TO 900
      ENDIF
      MDBASE = 10*(MDF/10)

C..... Check for file routing; use its decade base number if it exists;
C      exit if product receipt is suspended
      CPROD = CKWP('DIALPROD',1,' ')
      IF (CPROD .NE. ' ') THEN
        IRTE = GETRTE (MDBASE,INEW,CNEW)
        IF (IRTE .LT. 0) THEN
          CALL EDEST ('Product ROUTEing SUSpended',0)
          GO TO 900
        ENDIF
        MDBASE = 10*(INEW/10)
      ENDIF

C..... Get schema name
      CSCHE = CPP(2,' ')
      IF (CSCHE .EQ. ' ') THEN
        CALL EDEST('SCHEMA name MUST be specified',0)
        GO TO 900
      ENDIF
      ISCHE = LIT(CSCHE)

C..... Get first data input data record

50    ISTAT = NLDNIN(ISEC,NANO,XLAT,XLON,SGNL,MULT,SEMI,ECCEN,ANG,XCHSQ)
      IF (ISTAT .LT. 0) THEN               ! EOF reached (no more data)
        CALL EDEST('NLDN2MD -- Failed on first flash record read',0)
        GO TO 900
      ELSE IF (ISTAT .EQ. 0) THEN
        GOTO 50
      ENDIF
      ISTAT = mcsectodaytime(ISEC, IDAY, ITIME)     ! <<<<< UPC mod 981203 >>>>>
      XLON  = -XLON

C..... Calculate the MD file number
      JDAY = MOD(IDAY,10)
      IF (JDAY .EQ. 0) JDAY = 10
      MDF = MDBASE + JDAY

C..... Check for existence of output MD file
      ISTAT = MDINFO(MDF,IHEDR)
      IF (ISTAT .EQ. 0) THEN              ! File exists
        IF (IHEDR(1) .NE. ISCHE) THEN     ! Verify schema match
          CALL EDEST('SCHEMA MISMATCH',0)
          CALL EDEST('MD FILE SCHEMA IS: '//CLIT(IHEDR(1))//
     +              ' ...BUT USER SCHEMA IS: '//CSCHE,0)
          GO TO 900
        ENDIF
        NC     = IHEDR(5)               ! # COLS allowed in file
        NDKEYS = IHEDR(9)               ! # keys in DATA portion
        ID     = IHEDR(16)              ! File ID (DAY of data)
        NDOFF  = IHEDR(30)              ! # offset to data portion
        IEOF   = IHEDR(31)              ! Last word in file

        MREC  = (IEOF - NDOFF) / NDKEYS ! # of flashes stored in file
        IROW  = MREC / NC + 1           ! ROW of last flash
        ICOL  = MOD(MREC,NC) + 1        ! COL of last flash

        NTOK  = -1                      ! Get scales, units, and locations
        NKEYS = MDKEYS(MDF,NTOK,LIST,SCALES,UNITS,LOCS)

        ISTAT = MDI(MDF,IROW,0,IHEDR(7),LOCS,IBUF)
        JDATE = IBUF(1)                 ! DAY in last ROW
        JTIME = IBUF(2)                 ! TIME in last ROW
        CALL SDEST('Appending to MD file:  ',MDF)
      ELSE                              ! Make MD file and check validity
        IVSN = 0
        NR   = 0
        NC   = 0
        ID   = 0
        ISTAT = mccydtostr(IDAY, 4, CTITLE(22:)) ! <<<<< UPC mod 981203 >>>>>
        ISTAT = MDMAKE(MDF,ISCHE,IVSN,NR,NC,ID,TITLE)
        IF (ISTAT .EQ. -1) THEN
          IF (IRTE .GT. 0) THEN
            ISTAT = -1
            CALL FSHRTE (IRTE,ISTAT)
          ENDIF
          CALL EDEST('ERROR creating MD file: ',MDF)
          GO TO 900
        ENDIF
        CALL SDEST('Created MD file:  ',MDF)
        ISTAT = m0mdtimeput(MDF,IDAY,0,IDAY,235959) ! <<<<< UPC mod 981203 >>>>>
        JDATE = IDAY
        JTIME = ITIME
        ICOL  = 1
        IROW  = 1
      ENDIF

C..... At this point, the MD file exists, get its name
      CALL MDNAME (MDF,RTEXT)
      CALL MOVWC (RTEXT,CFILE)

C..... Get pertinent information about file
      ISTAT  = MDINFO(MDF,IHEDR)
      NR     = IHEDR(4)               ! # ROWS allowed in file
      NC     = IHEDR(5)               ! # COLS allowed in file
      NKEYS  = IHEDR(6)               ! # keys in file
      NRKEYS = IHEDR(7)               ! # keys in ROW header
      NCKEYS = IHEDR(8)               ! # keys in COL header
      NDKEYS = IHEDR(9)               ! # keys in DATA portion
      NCPOS  = IHEDR(10)              ! # position of column header
      NDPOS  = IHEDR(11)              ! # position of data portion
      NDOFF  = IHEDR(30)              ! # offset to data portion

C..... Get the KEYS, SCALES, and UNITS from the MD file
      NTOK  = -1
      NKEYS = MDKEYS(MDF,NTOK,LIST,SCALES,UNITS,LOCS)
      IF (NKEYS .LE. 0) THEN
        CALL EDEST('NO KEYS FOUND IN MD FILE: ',NKEYS)
        GO TO 900
      ENDIF

C
C..... Main loop:  Input values, find position in MD file; write values out
C

C..... Pack the output record: convert character data to integer, and scale
C      floating point data by factor in schema

100   RECORD(4) = ITIME
      RECORD(5) = IDNINT(XLAT*(10.D0**SCALES(5)))
      RECORD(6) = IDNINT(XLON*(10.D0**SCALES(6)))
      RECORD(7) = IDNINT(SGNL*(10.D0**SCALES(7)))
      RECORD(8) = MULT

C..... Determine if data should go in this file

      IF (MOD(IDAY,100000) .NE. MOD(JDATE,100000)) THEN ! Date <> file date

        IF (NREC .GT. 0) THEN              ! Has data been written?
          ISTAT = LWO(CFILE,15,1,JDATE)    ! Use YYDDD as internal file ID
          NFLSH = NC*(JROW-1) + JCOL       ! # of flashes stored in file
          NPOS  = NDKEYS*NFLSH + NDOFF     ! Position of next write
          ISTAT = LWO(CFILE,30,1,NPOS)     ! First unused word in file
          call ddest ('writing row header: ',irow)
          RECORD(1) = JDATE                ! DAY of data in ROW
          RECORD(2) = JTIME                ! Last time in ROW
          RECORD(3) = JCOL                 ! # COLs in ROW
          ISTAT = MDOIMP(MDF,JROW,0,NRKEYS,LOCS,RECORD)
          IF (ISTAT .NE. 0) THEN
            CALL EDEST('ERROR WRITING ROW HEADER TO MD FILE: ',MDF)
            CALL EDEST('MD FILE SIZE MISMATCH',0)
            ISTAT = -1
            GOTO 300
          ENDIF
          CALL SDEST('NLDN2MD - Flash events in file: ',NFLSH)
        ENDIF

        JDAY = MOD (IDAY,10)
        IF (JDAY .EQ. 0) JDAY = 10
        MDF = MDBASE + JDAY

        ISTAT = MDINFO(MDF,IHEDR)          ! Check for MD file existence
        IF (ISTAT .EQ. 0) THEN             ! File exists

          IF (IHEDR(1) .NE. ISCHE) THEN
            CALL EDEST('SCHEMA MISMATCH',0)
            CALL EDEST('MD FILE SCHEMA IS: '//CLIT(IHEDR(1))//
     +              ' ...BUT USER SCHEMA IS: '//CSCHE,0)
            GO TO 900
          ENDIF

          NR     = IHEDR(4)               ! # ROWS allowed in file
          NC     = IHEDR(5)               ! # COLS allowed in file
          NKEYS  = IHEDR(6)               ! # keys in file
          NRKEYS = IHEDR(7)               ! # keys in ROW header
          NCKEYS = IHEDR(8)               ! # keys in COL header
          NDKEYS = IHEDR(9)               ! # keys in DATA portion
          NCPOS  = IHEDR(10)              ! # position of column header
          NDPOS  = IHEDR(11)              ! # position of data portion
          ID     = IHEDR(16)              ! File ID (DAY of data)
          NDOFF  = IHEDR(30)              ! # offset to data portion
          IEOF   = IHEDR(31)              ! Last word in file
 
          MREC  = (IEOF - NDOFF) / NDKEYS ! # of flashes stored in file
          IROW  = MREC / NC + 1           ! ROW of last flash
          ICOL  = MOD(MREC,NC) + 1        ! COL of last flash
 
          NTOK  = -1                      ! Get scales, units, and locations
          NKEYS = MDKEYS(MDF,NTOK,LIST,SCALES,UNITS,LOCS)
 
          ISTAT = MDI(MDF,IROW,0,IHEDR(7),LOCS,IBUF)
          JDATE = IBUF(1)                 ! DAY in last ROW
          JTIME = IBUF(2)                 ! TIME in last ROW
          CALL SDEST('Appending to MD file:  ',MDF)

        ELSE                              ! File doesn't exists; create it
          IVSN = 0
          NR   = 0
          NC   = 0
          ID   = 0
          ISTAT = mccydtostr(IDAY, 4, CTITLE(22:)) ! <<<<< UPC mod 981203 >>>>>
          ISTAT = MDMAKE(MDF,ISCHE,IVSN,NR,NC,ID,TITLE)
          IF (ISTAT .EQ. -1) THEN
            CALL EDEST('ERROR creating MD file: ',MDF)
            ISTAT = -1
            GOTO 300
          ENDIF
          CALL SDEST('Created MD file:  ',MDF)
          ISTAT = m0mdtimeput(MDF,IDAY,0,IDAY,235959) ! <<<<< UPC mod 981203 >>>
          CALL MDNAME (MDF,RTEXT)
          CALL MOVWC (RTEXT,CFILE)
          ISTAT  = MDINFO(MDF,IHEDR)
          NR     = IHEDR(4)               ! # ROWS allowed in file
          NC     = IHEDR(5)               ! # COLS allowed in file
          NKEYS  = IHEDR(6)               ! # keys in file
          NRKEYS = IHEDR(7)               ! # keys in ROW header
          NCKEYS = IHEDR(8)               ! # keys in COL header
          NDKEYS = IHEDR(9)               ! # keys in DATA portion
          NCPOS  = IHEDR(10)              ! # position of column header
          NDPOS  = IHEDR(11)              ! # position of data portion
          IROW   = 1
          ICOL   = 1
          JDATE  = IDAY
          JTIME  = 0
        ENDIF
        NREC = 0

      ENDIF

C..... Determine correct COL for write

      IF (ICOL .GT. NC) THEN
        IF (NREC .GT. 0) THEN
          call ddest ('writing row header: ',jrow)
          RECORD(1) = JDATE                ! DAY of data in ROW
          RECORD(2) = JTIME                ! Last time in ROW
          RECORD(3) = JCOL                 ! # COLs in ROW
          ISTAT = MDOIMP(MDF,JROW,0,NRKEYS,LOCS,RECORD)
          IF (ISTAT .NE. 0) THEN
            CALL EDEST('ERROR WRITING ROW HEADER TO MD FILE: ',MDF)
            CALL EDEST('MD FILE SIZE MISMATCH',0)
            IF (IRTE .GT. 0) THEN
              ISTAT = -1
              CALL FSHRTE (IRTE,ISTAT)
            ENDIF
            GO TO 900
          ENDIF
        ENDIF
        IROW = IROW + 1
        IF (IROW .GT. NR) THEN
          CALL EDEST('NLDN2MD -- SCHEMA ROW limit exceeded',0)
          IF (IRTE .GT. 0) THEN
            ISTAT = -1
            CALL FSHRTE (IRTE,ISTAT)
          ENDIF
          GO TO 900
        ENDIF
        ICOL = 1
      ENDIF

C..... Write data to the MD file
      CALL DDEST ('Writing data: ',ICOL)
      ISTAT = MDOIMP(MDF,IROW,ICOL,NKEYS,LOCS,RECORD)
      IF (ISTAT .NE. 0) THEN
        CALL EDEST('ERROR WRITING DATA TO MD FILE: ',MDF)
        CALL EDEST('MD FILE SIZE MISMATCH',0)
        IF (IRTE .GT. 0) THEN
          ISTAT = -1
          CALL FSHRTE (IRTE,ISTAT)
        ENDIF
        GO TO 900
      ENDIF
      JROW  = IROW
      JCOL  = ICOL
      JTIME = ITIME
      IF (ICOL .EQ. 1) THEN
        JDATE = IDAY
      ENDIF
      NREC = NREC + 1

C..... Get another input record

150   ISTAT = NLDNIN(ISEC,NANO,XLAT,XLON,SGNL,MULT,SEMI,ECCEN,ANG,XCHSQ)
      IF (ISTAT .LT. 0) THEN               ! no more data (EOF)
        GOTO 200
      ELSE IF (ISTAT .EQ. 0) THEN
        GOTO 150
      ENDIF
      ISTAT = mcsectodaytime(ISEC, IDAY, ITIME)     ! <<<<< UPC mod 981203 >>>>>
      XLON  = -XLON

      ICOL = ICOL + 1
      GOTO 100

C..... Write the ROW header IF data has been written to the file.

200   IF (NREC .GT. 0) THEN
        ISTAT = LWO(CFILE,15,1,JDATE)    ! Use YYDDD as internal file ID
        NPOS  = NDKEYS*(NC*(JROW-1) + JCOL) + NDOFF
        ISTAT = LWO(CFILE,30,1,NPOS)     ! First unused word in file
        CALL DDEST ('Writing ROW header: ',JROW)
        RECORD(1) = JDATE                ! DAY of data in ROW
        RECORD(2) = JTIME                ! Last time in ROW
        RECORD(3) = JCOL                 ! # COLs in ROW
        ISTAT = MDOIMP(MDF,JROW,0,NRKEYS,LOCS,RECORD)
        IF (ISTAT .NE. 0) THEN
          CALL EDEST('ERROR WRITING ROW HEADER TO MD FILE: ',MDF)
          CALL EDEST('MD FILE SIZE MISMATCH',0)
          ISTAT = -1
          GOTO 300
        ENDIF
      ENDIF
      NFLSH = NC*(JROW-1) + JCOL       ! # of flashes stored in file
      CALL SDEST('NLDN2MD - Flash events in file: ',NFLSH)
      ISTAT = 1

C..... Finished

300   IF (IRTE .GT. 0) THEN
        RCDAY = IDAY                      ! Set the times in the Post-Process
        RCTIM = ITIME                     ! BATCH file to latest data
        CALL FSHRTE (IRTE,ISTAT)
      ELSE                                ! Update System Key Table values
        CALL SYSIN(2051,MDF)              ! if ROUTE.SYS is not used
        CALL SYSIN(2053,IDAY)
      ENDIF
      CALL SYSIN(2052,ITIME)
      CALL SDEST('NLDN2MD -- DONE ',0)

900   RETURN
      END

c      INTEGER FUNCTION NLDNIN(ISEC,NANO,XLAT,XLON,SGNL,MULT,SEMI,
c     +                        ECCEN,ANG,XCHSQ)
c
c      IMPLICIT NONE
c
c      INTEGER      IOPN, ISTAT
c      INTEGER      ISEC, MULT, NANO
c      REAL*4       ANG, ECCEN, SEMI, SGNL, XCHSQ, XLAT, XLON
c
c      DATA IOPN /0/
c
c      IF (IOPN .NE. 1) THEN
c        OPEN(10,FILE='nldn.dat',STATUS='OLD',IOSTAT=ISTAT,ERR=100)
c100     IF (ISTAT .NE. 0) THEN
c          NLDNIN = -1
c          RETURN
c        ELSE
c          IOPN = 1
c        ENDIF
c      ENDIF
c
c200   READ(10,800,IOSTAT=ISTAT,ERR=210) ISEC,NANO,XLAT,XLON,SGNL,
c     +                                  MULT,SEMI,ECCEN,ANG,XCHSQ
c210   IF (ISTAT .NE. 0) THEN
c        NLDNIN = -1
c        RETURN
c      ENDIF
c
c      SGNL = SGNL / 10.0
c300   NLDNIN = 1
c      RETURN
c
c800   FORMAT(2(I6,1X),2(F9.4,1X),I5,1X,I2,1X,3(I4,1X),I2)
c      END

      INTEGER FUNCTION MDOIMP(MDNO,M,N,NPTRS,PTRS,ARRAY)
C *** McIDAS Revision History ***
C 1 MDO.FOR 23-Mar-90,13:18:08,`SSEC' PC-McIDAS ver 5.00
C 2 MDO.FOR 1-Oct-90,13:14:10,`RCD' First Release into COMmon
C *** McIDAS Revision History ***
C $ FUNCTION MDO(MDNO, M, N, NPTRS, PTRS, ARRAY)  (GAD)
C $ OUTPUT DATA BY KEY (I.E. POINTER) FROM MD FILE. FN VAL IS 0 (OK),
C $   -1 (A RECORD COMPONENT IS MISSING OR ELSE (M,N) EXCEEDS THE FILE
C $   BOUNDS (THE MD MATRIX SIZE)).
C $ MDNO = (I) INPUT  MD FILE #
C $ M, N = (I) INPUT  ROW,COLUMN #'S OF DESIRED RECORD. IF M=0, A COLUMN
C $   HEADER IS DESIGNATED. IF N=0, A ROW HEADER IS DESIGNATED.
C $ NPTRS = (I) INPUT  NUMBER OF DATA ITEMS TO RETRIEVE FROM FILE
C $   IF NEGATIVE, THE ENTIRE ROW HDR + COL HDR + DATA PORTION IS
C $   COPIED INTO 'ARRAY', AND 'PTRS' IS IGNORED.
C $ PTRS = (I) INPUT  ARRAY (NPTRS LONG) OF SUBSCRIPTS WITHIN RECORD
C $   OF DATA DESIRED TO EXTRACT.
C $ ARRAY = (I) OUTPUT  NPTRS LONG, RECEIVES DATA
C $$ MDO = MD, I/O
C
C-----NOTE ON THE CODING: THE PROGRAM IS LARGELY IN 'OPEN CODE'
C-----   FORM (MUCH DUPLICATION) FOR FASTER EXECUTION (AT THE COST OF
C-----  SOME SPACE).
C
C
      IMPLICIT INTEGER (A-Z)
      INTEGER PTRS(*)
      INTEGER ARRAY(*)
      INTEGER FNAM(2)
      CHARACTER*12 CNAME
      INTEGER MDHD(64)
      INTEGER BUF(400)
      COMMON/CURRMD/CNAME              ! <<<<< UPC mod 951023 >>>>>
C
C
C-----LAYOUT OF MD FILE HEADER
C        FILE HEADER IS FIRST 64 WORD OF MD FILE
C        VARIABLES ARE SHOWN EQUIVALENCED TO 64-WORD ARRAY 'MDHD'
C
C
C
C-----SNAME,SVSN,SDATE GIVE THE SCHEMA NAME, VERSION #, & DATE
      EQUIVALENCE (SNAME,MDHD(1)),(SVSN,MDHD(2)),(SDATE,MDHD(3))
C-----NR,NC ARE THE # OF ROWS & COLUMNS IN THIS FILE
      EQUIVALENCE (NR,MDHD(4)),(NC,MDHD(5))
C-----NKEYS IS THE TOTAL # OF RECORD KEYS (= RECORD LENGTH, IN WORDS)
C-----NRKEYS,NCKEYS,NDKEYS GIVE THE # OF KEYS IN THE ROW HEADER, COL
C-----   HEADER, AND DATA PORTION, RESP. SUM MUST BE = NKEYS
      EQUIVALENCE (NKEYS,MDHD(6)),(NRKEYS,MDHD(7)),
     *   (NCKEYS,MDHD(8)),(NDKEYS,MDHD(9))
C-----CPOS,DPOS GIVES (1-BASED) POS IN RECORD OF COL HDR, DATA PORTION
      EQUIVALENCE (CPOS,MDHD(10)),(DPOS,MDHD(11))
C-----NREPS,REPSIZ,REPPOS GIVES # OF REPEAT GROUPS (NULL CASE IS 1),
C-----   SIZE OF REP. GP (NULL CASE =NDKEYS) AND START OF GP (NULL=DPOS)
      EQUIVALENCE (NREPS,MDHD(12)),(REPSIZ,MDHD(13)),(REPPOS,MDHD(14))
C-----MISSING DATA CODE (BY 7/82 CONVENTION = Z80808080)
      EQUIVALENCE (MDATA,MDHD(15))
C-----ID,TEXTID (8-WORD ARRAY) ARE INTEGER AND TEXT ID'S FOR FILE
      EQUIVALENCE (ID,MDHD(16)),(TEXTID,MDHD(17))
C-----CPROJ,CDATE,COWNER ARE CREATOR'S PROJECT,CREATE DATE, & TERMINAL
      EQUIVALENCE (CPROJ,MDHD(25)),(CDATE,MDHD(26)),(COWNER,MDHD(27))
C-----0-BASED WORD OFFSETS INTO MD FILE WHERE CAN BE FOUND THE FIRST
C-----   WORD OF THE FIRST: ROW HEADER, COL HEADER, DATA PORTION,
C-----   END-OF-MDFILE-DATA (EVERYTHING BEYOND HERE AVAILABLE TO USER),
C-----   400-WORD USER RECORD,KEYNAME LIST, KEYSCALE LIST, KEYUNITS LIST
      EQUIVALENCE (FRHOF,MDHD(28)),(FCHOF,MDHD(29)),(FDATOF,MDHD(30))
      EQUIVALENCE (FENDOF,MDHD(31)),(FUSROF,MDHD(32))
      EQUIVALENCE (FNAMOF,MDHD(33)),(FSCAOF,MDHD(34)),(FUNIOF,MDHD(35))
C-----REMAINING 29 WORDS UNUSED (ON DISK)
C
C
C
C
C-----CF,FNAM ETC. KEEP TRACK OF MDI/MDO'S CURRENT STATE
C-----   CF IS CURRENT FILE #, FNAM IS CUR. FILE NAME
C-----   CRH,CCH ARE CUR. ROW HEADER, COL. HEADER IN BUFFER
C-----   CR,CC ARE ROW,COL LAST READ INTO BUFFER
      EQUIVALENCE (FNAM,MDHD(62))
      EQUIVALENCE (CRH,MDHD(58)),(CCH,MDHD(59)),(CR,MDHD(60)),
     *  (CC,MDHD(61))


      DATA CF/-999988/
C
C
      IF (MDNO.EQ.CF) GOTO 13
C-----"OPEN" FILE; IE, GET NECESSARY HEADER DATA FROM MD FILE
      IF (MDINFO(MDNO,MDHD).NE.0) GOTO 95

      CNAME=' '
      CALL MOVWC(FNAM,CNAME(1:8))
      CF=MDNO

 13   CONTINUE
      IF (NPTRS.LE.0) GOTO 94
      IF (M.GT.NR.OR.N.GT.NC) GOTO 95
      IF (N.LE.0) GOTO 14
      IF (M.LE.0) GOTO 17
      GOTO 20
C-----WRITE ROW HEADER
 14   LL=FRHOF+(M-1)*NRKEYS
      IF (M.EQ.CRH) GOTO 15
      IF (M.LE.0) GOTO 95
      CRH=M
      III=LWI(CNAME,LL,NRKEYS,BUF)
 15   DO 16 J=1,NPTRS
         K=PTRS(J)
         BUF(K)=ARRAY(J)
 16   CONTINUE
      IST=LWO(CNAME,LL,NRKEYS,BUF)
      GOTO 94
C-----WRITE COLUMN HEADER
 17   LL=FCHOF+(N-1)*NCKEYS
      IF (N.EQ.CCH) GOTO 18
      CCH=N
      III=LWI(CNAME,LL,NCKEYS,BUF(CPOS))
 18   DO 19 J=1,NPTRS
         K=PTRS(J)
         IF (K.GT.0) BUF(K)=ARRAY(J)
 19   CONTINUE
      IST=LWO(CNAME,LL,NCKEYS,BUF(CPOS))
      GOTO 94
C-----WRITE DATA PORTION
 20   CONTINUE
c20   LL=FDATOF+((CR-1)*NC+CC-1)*NDKEYS
c     IF (M.EQ.CR.AND.N.EQ.CC) GOTO 21
      CR=M
      CC=N
      LL=FDATOF+((CR-1)*NC+CC-1)*NDKEYS
c     III=LWI(CNAME,LL,NDKEYS,BUF(DPOS))
 21   DO 22 J=1,NPTRS
         K=PTRS(J)
         BUF(K)=ARRAY(J)
 22   CONTINUE
      IST=LWO(CNAME,LL,NDKEYS,BUF(DPOS))
      GOTO 94
 94   MDOIMP=0
      RETURN
 95   MDOIMP=-1
      RETURN
      END
