/***************************************************************************/
/*                                                                         */
/* Modul: Edit89.H                                                         */
/* Autor: Michael Schmidt                                                  */
/* Datum: 17.12.1994                                                       */
/*                                                                         */
/***************************************************************************/

/*---- Funktionsprototypen ------------------------------------------------*/
BOOL InitWindows(HWND hwnd);                /* Fenster initialisieren      */
BOOL ImportData(HWND hwnd, ULONG ulDrive, ULONG ulSector, ULONG ulSecCount,
   ULONG ulEditMode);                       /* Sektor(en) ins MLE bringen  */
BOOL ExportData(HWND hwnd, ULONG ulDrive, ULONG ulSector, ULONG ulSecCount,
   ULONG ulEditMode);                       /* Sektor(en) aus MLE abspei.  */
BOOL PaintOvlWindows(HWND hwnd, HPS hps, ULONG ulWinMode);
                                            /* Overlay-Fenster neu aufbau. */
USHORT ShowMessage(ULONG ulMessageId, ULONG ulFlags, ...);
                                            /* Meldung ausgeben            */

/*---- Konstanten fÅr Editiermodus ----------------------------------------*/
#define EDIT_LOG1            1
#define EDIT_LOG2            2
#define EDIT_ABS             3

/*---- Konstanten fÅr Meldungsausgabe -------------------------------------*/
#define SM_WARNING           0x00000001     /* Meldung als Warnung ausgeb. */
#define SM_OKCANCEL          0x00010000     /* OK- und Cancelbutton        */

#define MSG_NUMSECS          0              /* falsche Anzahl Sektoren     */
#define MSG_RDERR            1              /* Lesefehler                  */
#define MSG_CONFIRM          2              /* Abspeichern bestÑtigen      */
#define MSG_INCONS           3              /* Datenfeld inkonsistent      */
#define MSG_WRERR            4              /* Schreibfehler               */
#define MSG_WRENTRY          5              /* Eingabefeld inkorrekt       */
#define MSG_CVSERR           6              /* Umrechnungsfehler           */
#define MSG_HLPABOUT         7              /* öber... ausgeben            */

/*---- Ressourcenkonstanten -----------------------------------------------*/
#define ID_MAIN              1              /* Hauptfenster                */


/*---- AktionsmenÅ --------------------------------------------------------*/
#define ID_PARTITION         100
#define ID_PARTSEL           101
#define ID_SECSEL            102
#define ID_CONVERT           103

#define ID_INTERPRET         110
#define ID_INTP_HEX          111
#define ID_INTP_MBR          112
#define ID_INTP_PBR          113

#define ID_HELP              120
#define ID_HLP_ABOUT         121


/*---- Editierfeld --------------------------------------------------------*/
#define ID_EDIT_HEX          200
#define ID_EDIT_LOAD         201
#define ID_EDIT_SAVE         202
#define ID_EDIT_MBR          210
#define ID_EDIT_PBR          220

/*---- Partition auswÑhlen ------------------------------------------------*/
#define ID_PARTSEL_DLG       300
#define ID_PARTSEL_SELECT    310
#define ID_PARTSEL_LOGPART   320
#define ID_PARTSEL_PHYSPART  321
#define ID_PARTSEL_OK        330
#define ID_PARTSEL_CANCEL    331

/*---- Sektoren auswÑhlen -------------------------------------------------*/
#define ID_SECSEL_DLG        400
#define ID_SECSEL_STARTEDIT  410
#define ID_SECSEL_COUNTEDIT  411
#define ID_SECSEL_STARTTXT   412
#define ID_SECSEL_COUNTTXT   413
#define ID_SECSEL_ACCTXT     414
#define ID_SECSEL_IOCTL      420
#define ID_SECSEL_RDWR       421
#define ID_SECSEL_OK         430
#define ID_SECSEL_CANCEL     431

/*---- Sektoren umrechnen --------------------------------------------------*/
#define ID_CONV_DLG          500
#define ID_CONV_SELECT       510
#define ID_CONV_PARTTXT      520
#define ID_CONV_CAT8TXT      521
#define ID_CONV_CYLEDIT      530
#define ID_CONV_HEADEDIT     531
#define ID_CONV_SECEDIT      532
#define ID_CONV_CYLTXT       533
#define ID_CONV_HEADTXT      534
#define ID_CONV_SECTXT       535
#define ID_CONV_SRLEDIT      540
#define ID_CONV_HIDSECEDIT   541
#define ID_CONV_SRLTXT       542
#define ID_CONV_HIDSECTXT    543
#define ID_CONV_LOGPART      550
#define ID_CONV_PHYSPART     551
#define ID_CONV_RDWR         560
#define ID_CONV_IOCTL        561
#define ID_CONV_CHS2SEC      570
#define ID_CONV_SEC2CHS      571
#define ID_CONV_CANCEL       572

