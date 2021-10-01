/*
**
** Name:    readit
**
** Purpose: read input from stdin, reformat it, and write it to stdout
**
** Note:    in order for ldm-mcidas deocders to successfully kickoff
**          McIDAS ROUTE PostProcess BATCH files, a McIDAS-X session
**          MUST be running on the same machine as the LDM.  Please
**          refer to README.PP for more information on setting up
**          your system to enable ROUTE PP BATCH processing.
**
** History: 950814 - Created for hack to get ROUTE PostProcessing to
**                   work with ldm-mcidas decoders executed by the LDM
**          960430 - Updated to work with McIDAS-X Version 2.102
**                   ROUTE PP BATCH file invocations (TWIN=0 keyword added)
**          960604 - Rewritten to be a little more bulletproof
**
*/

#include <stdio.h>

main()
{
	char	input[161];
	char	*cp;

	(void) scanf("%160[^\001-\037\177-\377]",input);

	for (cp = input + sizeof(input); --cp > input; )
		if (cp[-1] != ' ')
			break;
	*cp = 0;

	(void) puts(input);
/*
	(void) printf("%s %ld\n",input,strlen(input));
*/
}
