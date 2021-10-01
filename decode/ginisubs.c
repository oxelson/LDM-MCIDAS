
/******************************** GiniFileName *******************************/

/*
** Name:       GiniFileName
**
** Purpose:    Create a string by expanding 'tokens' with replacement values
**
** Parameters:
**             imghed - GINI image header
**             fmt    - token-laden representation of string
**
** Notes:      Tokens can take the form of:
**
**             'strftime'     - format specifiers ('man strftime' for a list)
**             'replaceables' - the list of replaceables and their meanings:
**                              \RES  - AREA spacing in image lines between
**                                        consecutive AREA lines
**                              \BAND - image band number (satellite dependent)
**                              \SS   - sensor source (satellite number)
** Returns:
**             SUCCESS == 1
**             FAILURE == 0
**
*/

char *
GiniFileName( unsigned char *imghed, char *fmt )
{

    char        *format;
    char        *pathname;

    char         areastr[32]="";
    char         bandstr[32]="";
    char         resstr[32]="";
    char         satstr[32]="";

    char         satname[][20]  = { "           ",       "           ",
                                    "COMPOSITE",         "DMSP", 
                                    "ERS",               "           ",
                                    "GOES",              "           ",
                                    "           ",       "JERS",
                                    "GMS",               "           ",
                                    "METEOSAT",          "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "TIROS",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           " };

    char         goesarea[][20] = { "AK-REGIONAL",       "AK-NATIONAL",
                                    "ALTIMETER",         "TROP-DISCUSS", 
                                    "EAST-CONUS",        "NHEM-COMP",
                                    "GAC",               "HI-NATIONAL",
                                    "HI-REGIONAL",       "PRECIP-ESTIMATES",
                                    "SCATTEROMETER",     "LAC",
                                    "SOLAR-ENV-MONITOR", "SUPER-NATIONAL",
                                    "OCEAN-COLOR",       "PR-REGIONAL",
                                    "PR-NATIONAL",       "HRPT",
                                    "SOUNDINGS",         "SST",
                                    "WINDS",             "SSM-I",
                                    "WEST-CONUS",        "WEFAX",
                                    "ASOS-CLOUD",        "AVAILABLE" };

    char         comparea[][20] = { "AK-REGIONAL",       "AK-NATIONAL",
                                    "ALTIMETER",         "TROP-DISCUSS", 
                                    "EAST-CONUS",        "NHEM-MULTICOMP",
                                    "GAC",               "HI-NATIONAL",
                                    "HI-REGIONAL",       "PRECIP-ESTIMATES",
                                    "SCATTEROMETER",     "LAC",
                                    "SOLAR-ENV-MONITOR", "SUPER-NATIONAL",
                                    "OCEAN-COLOR",       "PR-REGIONAL",
                                    "PR-NATIONAL",       "HRPT",
                                    "SOUNDINGS",         "SST",
                                    "WINDS",             "SSM-I",
                                    "WEST-CONUS",        "WEFAX",
                                    "ASOS-CLOUD",        "NEXRCOMP" };

    char         elsearea[][20] = { "           ",       "           ",
                                    "           ",       "           ", 
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           ",
                                    "           ",       "           " };

    char         goesband[][20] = { "0.65",              "10.7",
                                    "12.0",              "3.9",
                                    "6.7",               "DER1",
                                    "DER2",              "DER3",
                                    "DER4",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "N0R",
                                    "    ",              "    ",
                                    "    ",              "    "};

    char         compband[][20] = { "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "   ",               "   ",
                                    "NET",               "N0R",
                                    "NCR",               "NVL",
                                    "N1P",               "NTP"};

    char         elseband[][20] = { "0.65",              "3.9",
                                    "6.7",               "10.7",
                                    "12.0",              "DER1",
                                    "DER2",              "DER3",
                                    "DER4",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "    ",
                                    "    ",              "N0R",
                                    "    ",              "    ",
                                    "    ",              "    "};

    int          band=0;
    int          ires=0;

    int          aindx;
    int          sindx;
    int          rc;

    struct tm    t={0};
    time_t       tsec=0;

    udebug( "in GiniFileName" );

    /*
    ** Create a string long enough to hold expanded file name
    */

    pathname = (char *) malloc( 256 );
    if ( pathname == (char *) NULL ) return( pathname );

    format = (char *) malloc( 256 );
    if ( format == (char *) NULL ) return( format );

    (void) strcpy( format, fmt );

    /*
    ** Figure out the Satellite, Coverage, Band, and Resolution
    **
    ** Supported headers look like:
    **
    **      TIG[A-Z][01-05]
    **      TICZ[00-99]
    */

    sindx = imghed[2] - 'A';                /* index into satname array      */
    aindx = imghed[3] - 'A';                /* index into xxxxarea array     */

    band  = imghed[24];                     /* Physical element/Channel ID   */
    ires  = imghed[62];                     /* Image Resolution [km]         */

    switch ( imghed[2] ) {

      case 'G':
               (void) strcpy( areastr, goesarea[aindx] );
               (void) strcpy( bandstr, goesband[band]  );
               break;

      case 'C':
               (void) strcpy( areastr, comparea[aindx] );
               (void) strcpy( bandstr, compband[band]  );
               break;

      case 'D':
      case 'E':
      case 'J':
      case 'K':
      case 'M':
      case 'T':
               (void) strcpy( areastr, elsearea[aindx] );
               (void) strcpy( bandstr, elseband[band]  );
               break;

      default:
               uerror( "Unsupported satellite: %c", imghed[2] );
               return( (char *) NULL );
    }

    (void) sprintf( resstr, "%dkm", ires );
    (void) strcpy( satstr,  satname[sindx]  );

    udebug( "sat:  %s", satstr  );
    udebug( "area: %s", areastr );
    udebug( "band: %s", bandstr );
    udebug( "res:  %s", resstr  );

    /*
    ** Replace tokens
    */

    rc = ReplaceToken( format, "\\AREA", areastr );;
    rc = ReplaceToken( format, "\\BAND", bandstr );
    rc = ReplaceToken( format, "\\RES",  resstr  );
    rc = ReplaceToken( format, "\\SAT",  satstr  );;

    /*
    ** Fill in the time structure with values from the image
    */

    if ( imghed[29] >= 70 )
      t.tm_year  = imghed[29];
    else
      t.tm_year  = 100 + imghed[29];
    t.tm_mon   = imghed[30]-1;
    t.tm_mday  = imghed[31];
    t.tm_hour  = imghed[32];
    t.tm_min   = imghed[33];
    t.tm_sec   = imghed[34];
    t.tm_isdst = -1;

    tsec = mktime( &t );

    t = *localtime( &tsec );

    rc = (int) strftime( pathname, 256, format, &t );

    if ( rc == 0 )
      return( (char *) NULL );
    else
      return( pathname );
}

