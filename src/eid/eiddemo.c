/*                                                           02.Feb.2010 v3.3
  ============================================================================

  EIDDEMO.C
  ~~~~~~~~~

  Description:
  ~~~~~~~~~~~~

  DEMO program to show the use of the error insertion device (EID),
  implementing:
  - bit error insertion: random to highly correlated (controlled by
    parameter gamma).
  - random frame erasure
  - TO BE IMPLEMENTED: burst frame erasure using the Bellcore module.

  The EID software needs a special data format. Each bit of the soft
  data bitstream is located in one 16-word, where the following
  definitions are assumed:

       Bit '0' is represented as '0x007F'
       Bit '1' is represented as '0x0081'

  This was defined to be compatible in the future with the so called
  'softbit' format, where the channel decoder outputs probabilities
  that the received bit is '0' or '1'. Frame boundaries are marked by
  a synchronism header, which is composed by a synchronism (SYNC) word
  defined as:

        SYNC-word = 0x6B21

  followed by a 16-bit, 2-complement word representing the number of
  softbits per frame (LFRAME). Therefore, the total number of 16-bit
  words (shorts) per frame is n+2. All the interfaces with the STL
  functions use LFRAME+2 as frame length parameter. These definition
  is compatible with the bitstream format in Annex B of ITU-T
  Recommendation G.192.

  A binary file format is used, where the first word on the file
  must be the SYNC-word, the second is the frame length word
  (equals to LFRAME), and then followed by LFRAME softbits
  (LFRAME=number of bits in one frame); then the next SYNC and
  frame length words, followed by the next frame, ... and so on.

  The program detects automatically the number of bits per frame
  by simply counting the bits between the first two SYNC headers.

  This program has been modified from the pre-STL96 version in
  order to handle G.192-compatible bitstreams. The pre-STL96
  sync headers consisted only of the sync word, what has been
  changed in the STL96 version of the STL.

  Software Tests:
  ~~~~~~~~~~~~~~~

  This software was tested at PKI under:
  * VAX/VMS
  * Turboc++ on a Compaq Deskpro 386/33L
  * Highc    on a Compaq Deskpro 386/33L

  With all three platforms/compilers identical results  have been
  produced processing about 100000 bits.


  Notes:
  ~~~~~~

  The error patterns in the EID are generated by a
  Gilbert-Elliot-Channel model. The random generator for
  producing equally distributed random numbers is  implemented as
  of type "linear congruential sequence".

  a) produce next random number (32 bit unsigned)
     seed = (69069*seed + 1)(modulo 2^32)

  b) return double value between 0.0 ... 1.0:
     RAN = seed/2^32

  If the function "open_eid" is called, the system time  of the
  current CPU is taken as the starting value for  for "seed". So,
  at every newstart a different error  pattern sequence will be
  produced.

  If one likes to save the complete state of the channel  (for
  example to reproduce results) he must store the  contents of the
  EID-struct at the end of his procedure.  In this DEMO-program it
  is shown, how the user may store  or recall the channel state
  from/to file.


  Compilation:
  ~~~~~~~~~~~~
	
  VAX/VMS:
  $ cc eiddemo
  $ link eiddemo
  $ eid   :== $eid_disk:[scd_dir]eiddemo
  $ eid

  Turbo-C, Turbo-C++:
  > tcc eiddemo
  > eiddemo

  HighC (MetaWare, version R2.32):
  > hc386 eiddemo.c
  > run386 eiddemo

  Sun-C (BSD-Unix)
  # cc -o eiddemo eiddemo.c -lm
  # eiddemo

  Original author:
  ~~~~~~~~~~~~~~~~

  Rudolf Hofmann
  PHILIPS KOMMUNIKATIONS INDUSTRIE AG Phone : +49 911 526-2603 
  Kommunikationssysteme		      FAX   : +49 911 526-3385 
  Thurn-und-Taxis-Strasse 14	      EMail : hf@pkinbg.uucp   
  D-8500 Nuernberg 10 (Germany)

  History:
  ~~~~~~~~
  28.Feb.1992  1.0  Release of 1st version to UGST <hf@pkinbg.uucp>
  22.Mar.1992  2.0  1st update due to changes in eid.c (v2.0)
                    <hf@pkinbg.uucp>
  30.Apr.1992  2.2  Change in program interface <tdsimao@cpqd.ansp.br>
  06.Jun.1995  2.3  Change in self-documentation to reflect compatibility
                    modification of the definition of the softbit values
                    of bits '1' and '0'. <simao@ctd.comsat.com>
  22.Feb.1996  2.4  Included ctype.h, fixed some small warnings, changed 
                    local prototypes to smart prototypes, changed
                    read/write system calls to ANSI fread/fwrite calls
                    <simao>
  07.Mar.1996  3.0  Modified to comply with the bitstream data 
                    representation in Annex B of
                    G.192. <simao@ctd.comsat.com>
  06.Oct.1997  3.1  Removed some compilation warnings <simao>
  13.Jan.1998  3.11 Clarified ambigous syntax in save_EID_to_file() <simao>
  28.Mar.2000  3.2  Added warning if module compiled in portability test
                    mode <simao.campos@labs.comsat.com>
  02.Feb.2010  3.3  Modified maximum string length for filename to avoid
                    buffer overruns (y.hiwasaki)
  ============================================================================
*/

/* ......... Includes ......... */
#include <stdio.h>              /* Standard I/O Definitions */
#include <ctype.h>              /* for toupper() */
#include <string.h>             /* for strcmp(), strcpy(), strlen() */
#include "ugstdemo.h"           /* general UGST definitions */
#include "eid.h"                /* EID functions */


/* .. Local function prototypes for saving/retrieving EID states in file .. */
void display_usage ARGS ((void));
long save_EID_to_file ARGS ((SCD_EID * EID, char *EIDfile, double ber, double gamma));
SCD_EID *recall_eid_from_file ARGS ((char *EIDfile, double *ber, double *gamma));
long READ_L ARGS ((FILE * fp, long n, long *longary));
long READ_lf ARGS ((FILE * fp, long n, double *doubleary));
long READ_c ARGS ((FILE * fp, long n, char *chr));

/* Local definitions */
#define SYNC_WORD (short)0x6B21
#ifdef STL92
#define OVERHEAD 1              /* Overhead is sync word */
#else
#define OVERHEAD 2              /* Overhead is sync word and length word */
#endif


/* ------------------------------------------------------------------------ */
/*                               Main Program                               */
/* ------------------------------------------------------------------------ */
int main (argc, argv)
     int argc;
     char *argv[];
{
  SCD_EID *BEReid;              /* pointer to EID-structure */
  SCD_EID *FEReid;              /* pointer to EID-structure */
  char BERfile[MAX_STRLEN];     /* file for saving bit error EID */
  char FERfile[MAX_STRLEN];     /* file for saving bit error EID */
  char ifile[MAX_STRLEN], ofile[MAX_STRLEN];
  FILE *ifilptr, *ofilptr;

  static int EOF_detected = 0;
  double FER;                   /* frame erasure rate */
  double BER;                   /* bit error rate */
  double BER_gamma, FER_gamma;  /* burst factors */

  long lseg;                    /* length of one transmitted frame */
  double ber1, fer1;            /* returns values from BER_generator */
  double ersfrms;               /* total distorted frames */
  double prcfrms;               /* number of processed frames */
  double dstbits;               /* total distorted bits */
  double prcbits;               /* number of processed bits */

  short *xbuff, *ybuff;         /* pointer to bit-buffer */
  short *EPbuff;                /* pointer to bit-buffer */
  short SYNCword, i;

  long smpno;                   /* samples read from file */
  char quiet = 0;
  clock_t t1, t2;
  double t;

#if defined(VMS)
  char mrs[15] = "mrs=512";     /* mrs definition for VMS */
#endif

#ifdef PORT_TEST
  extern int PORTABILITY_TEST_OPERATION;
  if (PORTABILITY_TEST_OPERATION)
    fprintf (stderr, "WARNING! %s: compiled for PORTABILITY tests!\n\a", argv[0]);
#endif


  /* ......... INITIALIZATIONS ......... */
  t = 0;
  t1 = 0;


  /* ......... DISPLAY INFOS ......... */
  printf ("\n ** Error Insertion Device Demo Program - 02/Feb/2010 v3.3 **\n");


  /* ......... GET PARAMETERS ......... */

  /* Check options */
  if (argc < 2)
    display_usage ();
  else {
    while (argc > 1 && argv[1][0] == '-')
      if (strcmp (argv[1], "-q") == 0) {
        /* Define resolution */
        quiet = 1;

        /* Move arg{c,v} over the option to the next argument */
        argc--;
        argv++;
      } else if (strcmp (argv[1], "-h") == 0 || strcmp (argv[1], "-?") == 0) {
        /* Display help */
        display_usage ();
      } else {
        fprintf (stderr, "ERROR! Invalid option \"%s\" in command line\n\n", argv[1]);
        display_usage ();
      }
  }

  /* Get first parameter, open file and test for sync word and length */

  GET_PAR_S (1, "_File with input bitstream: ................ ", ifile);
  if ((ifilptr = fopen (ifile, RB)) == NULL)
    HARAKIRI ("    Could not open input file", 1);

  /* Check if first word in bit-stream file is a SYNC word */
  smpno = fread (&SYNCword, sizeof (SYNCword), 1, ifilptr);
  if (SYNCword != SYNC_WORD)
    HARAKIRI ("    First word on input file not the SYNC-word (0x6B21)", 1);

  /* Now find the number of bits per frame, lseg */
  for (lseg = -OVERHEAD, i = 0; i != SYNC_WORD; lseg++) {
    if ((smpno = fread (&i, sizeof (short), 1, ifilptr)) == 0)
      HARAKIRI ("    No next SYNC-word found on input file", 1);
  }

  /* move file pointer back to begin */
  fseek (ifilptr, 0L, 0);


  /* ... Continue with other parameters ... */

  GET_PAR_S (2, "_File for disturbed bitstream: ............. ", ofile);
  if ((ofilptr = fopen (ofile, WB)) == NULL)
    HARAKIRI ("    Could not create output file", 1);

  /* Ask for file with EID-States for INSERTING BIT ERRORS */
  GET_PAR_S (3, "_File with EID-states for bit errors: ...... ", BERfile);

  /* ... and file with EID-States for FRAME ERASURE MODULE */
  GET_PAR_S (4, "_File with EID-states for frame erasure: ... ", FERfile);

  /* ---------------------------------------------------------------------- Try to open file with EID-States for INSERTING BIT ERRORS. If failed (file does not exist), the user is asked to enter bit error rate and burst factor. If the file exists, the BIT ERROR INSERTION module continues with last states, stored on file (bit error rate and gamma are defined by the states, stored on that file. ---------------------------------------------------------------------- */
  BEReid = recall_eid_from_file (&BERfile[0], &BER, &BER_gamma);
  if (BEReid == (SCD_EID *) 0) {
    fprintf (stderr, "%s%s%c", "!!! File with EID-states (bit error insertion)", " does not exist: created one!\n", 7);
    GET_PAR_D (5, "__bit error rate     (0.0 ... 0.50): ....... ", BER);
    GET_PAR_D (6, "__burst factor       (0.0 ... 0.99): ....... ", BER_gamma);

    /* Setup a new EID */
    if ((BEReid = open_eid (BER, BER_gamma)) == (SCD_EID *) 0)
      HARAKIRI ("    Could not create EID for bit errors!?", 1);
    i = 7;                      /* next parameter number */
  } else {
    i = 5;                      /* next parameter number */
    fprintf (stderr, "__bit error rate on BER-file: .............. %f\n", BER);
    fprintf (stderr, "__burst factor BER_gamma: .................. %f\n", BER_gamma);
  }


  /* ---------------------------------------------------------------------- Find to open file with EID-States for FRAME ERASURE MODULE. If failed (file does not exist), the user is asked to enter frame erasure rate and burst factor. If the file exists, the FRAME ERASURE MODULE continues with last states, stored on file (frame erasure rate and gamma are defined by the states, stored on that file. ---------------------------------------------------------------------- */
  FEReid = recall_eid_from_file (&FERfile[0], &FER, &FER_gamma);
  if (FEReid == (SCD_EID *) 0) {
    fprintf (stderr, "%s%s%c", "!!! File with EID-states (frame ", "erasure) does not exist: created one!\n", 7);
    GET_PAR_D (i, "__Frame erasure rate (0.0 ... 0.50): ....... ", FER);
    if (FER != 0.0) {
      GET_PAR_D (i + 1, "__Burst factor       (0.0 ... 0.99): ....... ", FER_gamma);

      /* Initialize EID (frame erasure) */
      FEReid = open_eid (FER, FER_gamma);       /* Open EID for frame erasure */
      /* gamma=0 ==> random err.sequence */
      if (FEReid == (SCD_EID *) 0)
        HARAKIRI ("    Could not create EID for frame erasure module!?", 1);
    } else
      printf ("   FRAME ERASURE MODULE switched off\n");
  } else {
    fprintf (stderr, "__Frame erasure rate on FER-file: .......... %f\n", FER);
    fprintf (stderr, "__Burst factor FER_gamma: .................. %f\n", FER_gamma);
  }


/*
  * ......... Allocate memory for I/O-buffer .........
  */

  printf ("_Bit-frame length found on input file: ..... %ld\n", lseg);

  /* Buffer for data from input file: */
  xbuff = (short *) malloc ((OVERHEAD + lseg) * sizeof (short));
  if (xbuff == (short *) 0)
    HARAKIRI ("    Could not allocate memory for input bitstream buffer", 1);

  /* Buffer for output bitstream: */
  ybuff = (short *) malloc ((OVERHEAD + lseg) * sizeof (short));
  if (ybuff == (short *) 0)
    HARAKIRI ("    Could not allocate memory for output bitstream buffer", 1);

  /* Buffer for storing the error pattern: */
  if ((EPbuff = (short *) malloc ((lseg) * sizeof (short))) == (short *) 0)
    HARAKIRI ("    Could not allocate memory for error pattern buffer", 1);


/*
  * ......... Now process input file .........
  */

  /* initialize counters to compute bit error and frame erasure rates */
  ersfrms = 0.0;                /* number of erased frames */
  prcfrms = 0.0;                /* number of processed frames */
  dstbits = 0.0;                /* number of distorted bits */
  prcbits = 0.0;                /* number of processed bits */

  /* Read input soft bitstream frames of lenght lseg+OVERHEAD from file */
  /* while ((smpno=fread(xbuff, sizeof(short), (lseg+OVERHEAD)/sizeof(short), ifilptr)) == (lseg + OVERHEAD)/sizeof(short)) */
  while ((smpno = fread (xbuff, sizeof (short), (lseg + OVERHEAD), ifilptr)) == (lseg + OVERHEAD)) {
    if (xbuff[0] == SYNCword && EOF_detected == 0) {
      /* Start measuring CPU-time for this round */
      t1 = clock ();

      /* Generate error pattern ('hard'-bits) */
      ber1 = BER_generator (BEReid, lseg, EPbuff);
      dstbits += ber1;          /* count number of disturbed bits */
      prcbits += (double) lseg; /* count number of processed bits */

      /* Modify input bitstream according to the stored error pattern */
      BER_insertion (lseg + OVERHEAD, xbuff, ybuff, EPbuff);

      /* Apply frame erasure module if requested */
      if (FER != 0.0) {
        /* Subject bitstream to frame erasure ... */
        fer1 = FER_module (FEReid, lseg + OVERHEAD, ybuff, xbuff);
        ersfrms += fer1;        /* count number of erased frames */
        prcfrms += (double) 1;  /* count number of processed frames */
      } else {
        /* Copy processed bitstream without subjecting to frame erasure ... */
        for (i = 0; i < lseg + OVERHEAD; i++)
          xbuff[i] = ybuff[i];
      }

      /* Get partial timimg */
      t2 = clock ();
      t += (t2 - t1) / (double) CLOCKS_PER_SEC;

      /* and write disturbed bits to output file */
      smpno = fwrite (xbuff, sizeof (short), (lseg + OVERHEAD), ofilptr);

      /* Print frame info */
      if (!quiet)
        fprintf (stderr, "\r%.0f bits processed", prcbits);
    } else
      EOF_detected = 1;
  }

  if (EOF_detected == 1)
    printf ("   --- end of file detected (no SYNCword match) ---\n");
  printf ("\n");

/*
   * ......... Print time and message with measured bit error rate .........
   */

  /* Print frame info */
  if (quiet)
    fprintf (stderr, "\r%.0f bits processed", prcbits);

  printf ("CPU-time %f sec\r\n\n", t);

  if (prcbits > 0) {
    printf ("measured bit error rate    : %f\n", dstbits / prcbits);
    printf ("  (%.0f of %.0f bits distorted)\n", dstbits, prcbits);
  }

  if (prcfrms > 0) {
    printf ("measured frame erasure rate: %f\n", ersfrms / prcfrms);
    printf ("  (%.0f of %.0f frames erased) \n", ersfrms, prcfrms);
  }

/*
  * ...... Save EID-status to file for bit error and frame erasure EIDs ......
  */

  /* NB: the following file I/O is a user defined routine, so this function is located within this DEMO file -- see below */
  save_EID_to_file (BEReid, &BERfile[0], BER, BER_gamma);
  if (FER != 0.0)
    save_EID_to_file (FEReid, &FERfile[0], FER, FER_gamma);

#ifndef VMS                     /* return value to OS if not VMS */
  return 0;
#endif
}

/* .......................... End of main() .......................... */


/*
  ============================================================================

        void display_usage (void);
        ~~~~~~~~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Display usage of this demo program and exit;

        Return value:  None.
        ~~~~~~~~~~~~~

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
#define P(x) printf x
void display_usage () {
  char prompt;

  /* Choose a nice prompt */
#if defined(VMS)
  prompt = '$';
#elif defined(MSDOS)
  prompt = '>';
#else
  prompt = '#';
#endif

  P (("eiddemo.c Version 3.3 of 02.Feb.2010\n"));

  /* Print the proper usage */
  P (("  Usage: %c %s%s", prompt, "EID ifile ofile BERfile FERfile ", "[ BER BER_gamma FER FER_gamma]\n\n"));

  P (("\tifile      : binary file with  input bitstream\n"));
  P (("\tofile      : binary file with output bitstream\n"));
  P (("%s%s", "\tBERfile    : File, containing the EID-status ", "for bit error rate \n"));
  P (("%s%s", "\tFERfile    : File, containing the EID-status ", "for frame erasure module\n"));
  P (("\tBER        : bit error rate (0.0 ... 0.50)\n"));
  P (("\tBER_gamma  : burst factor   (0.0 ... 0.99)\n"));
  P (("\t\t         =0.00 --> errors are totally random\n"));
  P (("\t\t         =0.50 --> errors are slightly bursty\n"));
  P (("\t\t         =0.99 --> errors are totally bursty\n"));
  P (("\tFER        : frame erasure rate (0.0 ... 0.5)\n"));
  P (("\tFER_gamma  : burst factor   (0.0 ... 0.99)\n"));
  P (("\t\t         =0.00 --> erasures are totally random\n"));
  P (("\t\t         =0.50 --> errsures are slightly bursty\n"));
  P (("\t\t         =0.99 --> errsures are totally bursty\n\n"));

  exit (1);
}

/* .................. End of display_usage() ....................... */


/*
  ===========================================================================

        long save_EID_to_file (SCD_EID *EID, char *EIDfile, double BER,
        ~~~~~~~~~~~~~~~~~~~~~  double GAMMA);

        Description:
        ~~~~~~~~~~~~

        Save current states of EID to file. Data are stored on an
        ASCII file. May be that on some platforms this function must be
        slightly modified, but has worked nicely for the all tested.

        The contents of the EID-struct are stored on an ASCII file to
        allow the user observation or control or what  ever. The file
        should look like:
                         BER           = 0.020000
                         GAMMA         = 0.500000
                         RAN-seed      = 0x1db85ea6
                         Current State = G
                         GOOD->GOOD    = 0.980000
                         GOOD->BAD     = 1.000000
                         BAD ->GOOD    = 0.480000
                         BAD ->BAD     = 1.000000

        Parameters:
        ~~~~~~~~~~~

        SCD_EID *EID .......... EID-structure
        char *EIDfile ......... filename for saving the state
        double BER ............ bit error rate
        double GAMMA .......... burst factor

        Return value:
        ~~~~~~~~~~~~~
        Returns 1 if EID-state was successfully saved to file; 0 if failed.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

  ===========================================================================
*/
long save_EID_to_file (EID, EIDfile, BER, GAMMA)
     SCD_EID *EID;
     char *EIDfile;
     double BER, GAMMA;
{
  FILE *EIDfileptr;

  /* open specified ASCII file for "overwriting": */
  EIDfileptr = fopen (EIDfile, RWT);

  /* If failed, create new file: */
  if (EIDfileptr == NULL) {
    if ((EIDfileptr = fopen (EIDfile, WT)) == NULL)
      return (0L);
  }

  /* otherwise: set filepointer to beginning of file for overwriting */
  else {
    fseek (EIDfileptr, 0L, 0);
  }

  /* Since the selected bit error rate and burst factor cannot be seen from the transition matrix, these values are also stored in file (only for documentation purposes). */
  fprintf (EIDfileptr, "BER           = %f\n", BER);
  fprintf (EIDfileptr, "GAMMA         = %f\n", GAMMA);

  /* current state of random generator: */
  fprintf (EIDfileptr, "RAN-seed      = 0x%08lx\n", get_RAN_seed (EID));

  /* current state of GEC-model: */
  fprintf (EIDfileptr, "Current State = %c\n", get_GEC_current_state (EID));

  /* Save contents of Transition Matrix: */
  fprintf (EIDfileptr, "GOOD->GOOD    = %f\n", get_GEC_matrix (EID, 'G', 'G'));
  fprintf (EIDfileptr, "GOOD->BAD     = %f\n", get_GEC_matrix (EID, 'G', 'B'));
  fprintf (EIDfileptr, "BAD ->GOOD    = %f\n", get_GEC_matrix (EID, 'B', 'G'));
  fprintf (EIDfileptr, "BAD ->BAD     = %f\n", get_GEC_matrix (EID, 'B', 'B'));

  fclose (EIDfileptr);
  return (1L);
}

/* ....................... End of save_EID_to_file() ....................... */


/*
  ============================================================================

        SCD_EID *open_eid_from_file (char *EIDfile,
        ~~~~~~~~~~~~~~~~~~~~~~~~~~~  double *ber, double *gamma);

        Description:
        ~~~~~~~~~~~~

        Allocate memory for EID struct and load EID-states from
        previous call (which is saved on file) into a struct SCD_EID.

        Data are read from an ASCII file. May be that  on some platforms
        this function must be slightly modified, but has worked nicely
        for the all tested.

        Parameters:
        ~~~~~~~~~~~
        char *EIDfile ... file with EID states
        double *ber ..... pointer to "bit error rate"
        double *gamma ... pointer to burst factor


        Return value:
        ~~~~~~~~~~~~~
        Returns a pointer to a EID-state data structure; if failed, it
        will be a null pointer.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
SCD_EID *recall_eid_from_file (EIDfile, ber, gamma)
     char *EIDfile;
     double *ber;
     double *gamma;
{
  SCD_EID *EID;
  FILE *EIDfileptr;
  char chr;
  double thr;
  long seed;


  /* Open ASCII file with EID states */
  if ((EIDfileptr = fopen (EIDfile, RT)) == NULL)
    return ((SCD_EID *) 0);

  /* Load channel model parameters ber and gamma */
  READ_lf (EIDfileptr, 1L, ber);
  READ_lf (EIDfileptr, 1L, gamma);

  /* Now open EID with default values and update states afterwards from file */
  if ((EID = open_eid (*ber, *gamma)) == (SCD_EID *) 0)
    return ((SCD_EID *) 0);

  /* update EID-struct from file: seed for random generator */
  READ_L (EIDfileptr, 1L, &seed);
  set_RAN_seed (EID, (unsigned long) seed);     /* store into struct */

  /* update EID-struct from file: current state */
  READ_c (EIDfileptr, 1L, &chr);
  set_GEC_current_state (EID, chr);

  /* update EID-struct from file: threshold GOOD->GOOD */
  READ_lf (EIDfileptr, 1L, &thr);
  set_GEC_matrix (EID, thr, 'G', 'G');

  /* update EID-struct from file: threshold GOOD->BAD */
  READ_lf (EIDfileptr, 1L, &thr);
  set_GEC_matrix (EID, thr, 'G', 'B');

  /* update EID-struct from file: threshold BAD ->GOOD */
  READ_lf (EIDfileptr, 1L, &thr);
  set_GEC_matrix (EID, thr, 'B', 'G');

  /* update EID-struct from file: threshold BAD ->BAD */
  READ_lf (EIDfileptr, 1L, &thr);
  set_GEC_matrix (EID, thr, 'B', 'B');


  /* Finalizations */
  fclose (EIDfileptr);
  return (EID);
}

/* ..................... End of recall_eid_from_file() ..................... */


/*
  ============================================================================

        long READ_L (FILE *fp, long n, long *ary);
        ~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Read `n' longs from an EID-state file onto an array.

        Return value:
        ~~~~~~~~~~~~~
        Returns the number of longs read.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
long READ_L (fp, n, longary)
     FILE *fp;
     long n;
     long *longary;
{
  long i, ic;
  char c;
  char ch[16];


  while ((c = getc (fp)) != '=');
  for (i = 0; i < n; i++) {
    while (((c = getc (fp)) == 32) || (c == 9));

    ic = 0;
    while ((c != 32) && (c != 9) && (c != '\n') && (ic < 15)) {
      ch[ic++] = c;
      c = getc (fp);
    }
    ch[ic] = (char) 0;
    if ((ch[0] == '0') && (toupper ((int) ch[1]) == 'X')) {
      sscanf (&ch[2], "%lx", &longary[i]);
    } else {
      sscanf (ch, "%ld", &longary[i]);
    }
  }
  return (n);
}

/* ....................... End of READ_L() ....................... */


/*
  ============================================================================

        long READ_lf (FILE *fp, long n, double *doubleary);
        ~~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Read `n' doubles from an EID-state file onto an array.

        Return value:
        ~~~~~~~~~~~~~
        Returns the number of doubles read.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
long READ_lf (fp, n, doubleary)
     FILE *fp;
     long n;
     double *doubleary;
{
  long i, ic;
  char c;
  char ch[64];


  while ((c = getc (fp)) != '=');
  for (i = 0; i < n; i++) {
    while (((c = getc (fp)) == 32) || (c == 9));

    ic = 0;
    while ((c != 32) && (c != 9) && (c != '\n') && (ic < 63)) {
      ch[ic++] = c;
      c = getc (fp);
    }
    ch[ic] = (char) 0;
    sscanf (ch, "%lf", &doubleary[i]);
  }
  return (n);
}

/* ....................... End of READ_lf() ....................... */


/*
  ============================================================================

        long READ_c (FILE *fp, long n, char *chr);
        ~~~~~~~~~~~

        Description:
        ~~~~~~~~~~~~

        Read `n' doubles from an EID-state file onto an array.

        Return value:
        ~~~~~~~~~~~~~
        Returns the number of chars read.

        Author: <hf@pkinbg.uucp>
        ~~~~~~~

        History:
        ~~~~~~~~
        28.Feb.92 v1.0 Release of 1st version <hf@pkinbg.uucp>

 ============================================================================
*/
long READ_c (fp, n, chr)
     FILE *fp;
     long n;
     char *chr;
{
  long i;
  char c;


  while ((c = getc (fp)) != '=');
  for (i = 0; i < n; i++) {
    while (((c = getc (fp)) == 32) || (c == 9));
    *chr = c;
    while ((c = getc (fp)) != '\n');
  }
  return (n);
}

/* ....................... End of READ_c() ....................... */