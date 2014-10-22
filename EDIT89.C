/***************************************************************************/
/*                                                                         */
/* Modul: Edit89.C                                                         */
/* Autor: Michael Schmidt                                                  */
/* Datum: 17.12.1994                                                       */
/*                                                                         */
/***************************************************************************/

#define INCL_BASE
#define INCL_PM

#include <os2.h>                            /* OS/2-Deklarationen          */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "edit89.h"                         /* Konstanten                  */
#include "rdwr89.h"


/*---- globale Variablen --------------------------------------------------*/
PCHAR pchXferBuffer = NULL;                 /* Transferpuffer              */

/*---- modulglobale Variablen ---------------------------------------------*/
static HAB hab;                             /* Handle fÅr Anchor Block     */
static HMQ hmq;                             /* Handle fÅr Message Queue    */

static HWND hwndMainFrame;                  /* Handle fÅr Frame Window     */
static HWND hwndMain;                       /* Handle fÅr Client Window    */
static HWND hwndEdit;                       /* Handle fÅr Editier-Window   */
static HWND hwndButtonSave, hwndButtonLoad; /* Handles fÅr Druckknîpfe     */
static HWND hwndMBR;                        /* Handle fÅr MBR-Window       */
static HWND hwndPBR;                        /* Handle fÅr PBR-Window       */

static ULONG ulDrive;                       /* phys. oder log. Laufwerk    */
static ULONG ulLogDrive, ulLogDriveMap;     /* Infos Åber log. Laufwerke   */
static ULONG ulPhysDrive, ulPhysDriveNum;   /* Infos Åber phys. Laufwerke  */
static ULONG ulSector, ulSecCount;          /* zu les./schreib. Sektoren   */
static ULONG ulEditMode;                    /* Editiermodus                */
static ULONG ulWinMode;                     /* Fensterstatus               */


static PSZ pszMsgArray[] =                  /* Ausgabetexte                */
{
   "Anzahl Sektoren mu· zwischen 1 und 16 liegen!",
   "DOS-Fehler %u beim Lesen von Sektoren!",
   "Wollen Sie wirklich abspeichern?",
   "Abzuspeichernde Daten sind inkonsistent! Bitte Eingabefeld korrigieren!",
   "DOS-Fehler %u beim Schreiben von Sektoren!",
   "Format der Eingabefelder inkorrekt!",
   "DOS_Fehler %u bei Sektorumrechnung!",
   "EDIT89\n\n'OS/2 Inside' Disk-Editor\n\n"
   "Copyright (C) 1995 by 'OS/2 Inside' and Michael Schmidt"
};


/*---- Funktionsprototypen ------------------------------------------------*/
static MRESULT EXPENTRY MainWindowProc(HWND hwnd, USHORT msg, MPARAM mp1, 
   MPARAM mp2);
static MRESULT EXPENTRY OvlWindowProc(HWND hwnd, USHORT msg, MPARAM mp1,
   MPARAM mp2);
static MRESULT EXPENTRY PartSelProc(HWND hwnd, USHORT msg, MPARAM mp1,
   MPARAM mp2);
static MRESULT EXPENTRY SecSelProc(HWND hwnd, USHORT msg, MPARAM mp1,
   MPARAM mp2);
static MRESULT EXPENTRY ConvertProc(HWND hwnd, USHORT msg, MPARAM mp1,
   MPARAM mp2);
static VOID MainCommand(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
static BOOL QueryPartitions(VOID);
static BOOL SetListBox(HWND hwnd, ULONG ulFlags, ULONG ulDefDrive);
static BOOL SwitchWindows(ULONG ulWinID);
static BOOL ConvertSecToCHS(HWND hwnd, ULONG ulConvMode);
static BOOL ConvertCHSToSec(HWND hwnd, ULONG ulConvMode);

/***************************************************************************/
/*                                                                         */
/* Funktion: main                                                          */
/*                                                                         */
/***************************************************************************/

INT main 
(
   VOID
)

{
   ULONG ulCtlData;                         /* Dummy                       */

   QMSG qmsg;                               /* Message Struktur            */


   pchXferBuffer = calloc(1, SECTOR_SIZE);  /* Mindestgrî·e Transferpuffer */

   QueryPartitions();                       /* Partitionen erfragen        */

   /*---- Startwerte setzen -----------------------------------------------*/
   ulPhysDrive = 1;                         /* phys. Laufwerk initialis.   */
   ulLogDrive = ulDrive = 3;                /* Initial. auf C:             */
   ulSector = 0;                            /* Bootsektor zeigen           */
   ulSecCount = 1;                          /* nur einen Sektor zeigen     */
   ulEditMode = EDIT_LOG2;                  /* logische Laufwerke zeigen   */
   ulWinMode = ID_INTP_HEX;                 /* mit Hex-MLE starten         */


   hab = WinInitialize(0);

   hmq = WinCreateMsgQueue(hab, 0);         /* Message Queue aufbauen      */


   /*---- nun wird das Main Window erstellt -------------------------------*/
   ulCtlData = FCF_STANDARD;

   WinRegisterClass(hab, "Edit89",(PFNWP)MainWindowProc,
      CS_SIZEREDRAW | CS_CLIPCHILDREN, 0);

   hwndMainFrame = WinCreateStdWindow(HWND_DESKTOP, 0, &ulCtlData,
      "Edit89", "Edit89", 0, NULLHANDLE, ID_MAIN, &hwndMain);

   WinSetWindowPos(hwndMainFrame, NULLHANDLE, 0, 0, 620, 400,
      SWP_ACTIVATE | SWP_SHOW | SWP_SIZE | SWP_MOVE);
                                            /* Fenster zeigen              */

   /*- Message Dispatcher aufbauen ----------------------------------------*/
   while (WinGetMsg(hab, &qmsg, 0, 0, 0))
      WinDispatchMsg(hab, &qmsg);


   WinDestroyMsgQueue(hmq);                 /* PM abbauen                  */
   WinTerminate(hab);

   free(pchXferBuffer);                     /* Transferpuffer freigeben    */

   return 0;                                /* Compiler zufriedenstellen   */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: MainWindowProc                                                */
/*                                                                         */
/***************************************************************************/

static MRESULT EXPENTRY MainWindowProc      /* Hauptfensterfunktion        */
(
   HWND hwnd,                               /* Fensterhandle               */
   USHORT msg,                              /* Message                     */
   MPARAM mp1,                              /* Messageparameter            */
   MPARAM mp2
)

{
   switch (msg)                             /* welche Message?             */
   {
      case WM_CREATE:                       /* Client Window erzeugen      */

         /*---- MLE-Editierfenster aufbauen -------------------------------*/
         hwndEdit = WinCreateWindow(hwnd, WC_MLE, NULL,
            MLS_BORDER | MLS_VSCROLL | MLS_HSCROLL, 0, 0, 0, 0, hwnd, 
            HWND_TOP, ID_EDIT_HEX, NULL, NULL);

         /*---- Einlese-Druckknopffenster aufbauen ------------------------*/
         hwndButtonLoad = WinCreateWindow(hwnd, WC_BUTTON,
            "Sektor(en) neu ~einlesen", 0, 10, 10, 170, 30, hwnd, HWND_TOP,
            ID_EDIT_LOAD, NULL, NULL);

         /*---- Abspeicher-Druckknopffenster aufbauen ---------------------*/
         hwndButtonSave = WinCreateWindow(hwnd, WC_BUTTON,
            "Sektor(en) ~abspeichern", 0, 200, 10, 180, 30, hwnd, HWND_TOP,
            ID_EDIT_SAVE, NULL, NULL);

         /*---- Overlay-Fensterklasse registrieren ------------------------*/
         WinRegisterClass(hab, "Overlay", (PFNWP)OvlWindowProc,
            CS_SIZEREDRAW | CS_CLIPCHILDREN, 0);

         /*---- MBR-Anzeigefenster aufbauen -------------------------------*/
         hwndMBR = WinCreateWindow(hwnd, "Overlay", NULL, SS_BKGNDRECT,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_EDIT_MBR, NULL, NULL);

         /*---- Bootsektor-Anzeigefenster aufbauen ------------------------*/
         hwndPBR = WinCreateWindow(hwnd, "Overlay", NULL, SS_BKGNDRECT,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_EDIT_PBR, NULL, NULL);

         InitWindows(hwnd);                 /* Fenster initialisieren      */
         ImportData(hwnd, ulDrive, ulSector, ulSecCount, ulEditMode); 
                                            /* Sektoren in Fenster bringen */
         /*---- die Grî·e der Editierfenster wird dynamisch in WM_SIZE ----*/
         /*-----bestimmt. -------------------------------------------------*/

         break;

      case WM_SIZE:                         /* Fenstergrî·e festlegen      */
         {
            LONG lXSize = (LONG)SHORT1FROMMP(mp2);
            LONG lYSize = (LONG)SHORT2FROMMP(mp2);

            /*---- Grî·e der Editierfenster setzen und Fenster zeigen -----*/
            WinSetWindowPos(hwndEdit, HWND_TOP, 0, 50, lXSize, lYSize - 50,
               SWP_SIZE | SWP_MOVE);
            WinSetWindowPos(hwndMBR, HWND_TOP, 0, 0, lXSize, lYSize,
               SWP_SIZE | SWP_MOVE);
            WinSetWindowPos(hwndPBR, HWND_TOP, 0, 0, lXSize, lYSize,
               SWP_SIZE | SWP_MOVE);


            SwitchWindows(ulWinMode);       /* Fenster aktivieren          */
         }

         break;

      case WM_ERASEBACKGROUND:              /* Hintergrund lîschen         */
         return (MRESULT)TRUE;

      case WM_PAINT:                        /* Fensterinhalt fÅllen        */
         {
            HPS hps;                        /* Presentation Space Handle   */
            RECTL rc;                       /* Rechteckkoordinaten         */
          
            /*---- Fensterhintergrund setzen ------------------------------*/
            hps = WinBeginPaint(hwnd, 0, &rc);
            WinFillRect(hps, &rc, SYSCLR_WINDOW);
            WinEndPaint(hps);
         }

         break;

      case WM_COMMAND:                      /* Kommandofunktion auslîsen   */
         MainCommand(hwnd, msg, mp1, mp2);
         break;

      case WM_INITMENU:                     /* MenÅs initialisieren        */
         break;

      default:
         return WinDefWindowProc(hwnd, msg, mp1, mp2);
   }

   return (MRESULT)0; 
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: OvlWindowProc                                                 */
/*                                                                         */
/***************************************************************************/

static MRESULT EXPENTRY OvlWindowProc       /* Overlay-Fensterfunktion     */
(
   HWND hwnd,                               /* Fensterhandle               */
   USHORT msg,                              /* Message                     */
   MPARAM mp1,                              /* Messageparameter            */
   MPARAM mp2
)

{
   switch (msg)                             /* welche Message?             */
   {
      case WM_CREATE:                       /* Client Window erzeugen      */
         break;

      case WM_SIZE:                         /* Fenstergrî·e festlegen      */
         break;

      case WM_ERASEBACKGROUND:              /* Hintergrund lîschen         */
         return (MRESULT)TRUE;

      case WM_PAINT:                        /* Fensterinhalt fÅllen        */
         {
            HPS hps;                        /* Presentation Space Handle   */
            RECTL rc;                       /* Rechteckkoordinaten         */

            /*---- Fensterhintergrund setzen ------------------------------*/
            hps = WinBeginPaint(hwnd, 0, &rc);
            WinFillRect(hps, &rc, SYSCLR_WINDOW);
            PaintOvlWindows(hwnd, hps, ulWinMode);
                                            /* Fenster neu aufbauen        */
            WinEndPaint(hps);

         }

         break;

      case WM_COMMAND:                      /* Kommandofunktion auslîsen   */
         break;

      case WM_INITMENU:                     /* MenÅs initialisieren        */
         break;

      default:
         return WinDefWindowProc(hwnd, msg, mp1, mp2);
   }

   return (MRESULT)0; 
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: MainCommand                                                   */
/*                                                                         */
/***************************************************************************/

static VOID MainCommand                     /* Kommandofunktion bearbeiten */
(
   HWND hwnd,                               /* Fensterhandle               */
   USHORT msg,                              /* Message                     */
   MPARAM mp1,                              /* Messageparameter            */
   MPARAM mp2
)

{
   BOOL boReturn;                           /* RÅckgabewert                */


   switch (SHORT1FROMMP(mp1))               /* welcher MenÅpunkt?          */
   {
      case ID_PARTSEL:                      /* Partition auswÑhlen         */
         WinDlgBox(HWND_DESKTOP, hwndMainFrame, (PFNWP)PartSelProc, 
            NULLHANDLE, ID_PARTSEL_DLG, NULL);
                                            /* Partitionsdialog starten    */

         break;

      case ID_SECSEL:                       /* Sektoren auswÑhlen          */
         WinDlgBox(HWND_DESKTOP, hwndMainFrame, (PFNWP)SecSelProc,
            NULLHANDLE, ID_SECSEL_DLG, NULL);
                                            /* Sektordialog starten        */

         break;

      case ID_CONVERT:                      /* Sektoren umrechnen          */
         WinDlgBox(HWND_DESKTOP, hwndMainFrame, (PFNWP)ConvertProc,
            NULLHANDLE, ID_CONV_DLG, NULL); /* Umrechnungsdialog starten   */

         break;

      case ID_EDIT_LOAD:                    /* Sektor(en) einlesen         */
         ImportData(hwnd, ulDrive, ulSector, ulSecCount, ulEditMode);
                                            /* Sektoren in Fenster bringen */

         SwitchWindows(ulWinMode = ID_INTP_HEX);
                                            /* in's MLE-Fenster zurÅcksch. */

         WinSetFocus(HWND_DESKTOP, hwndEdit);
                                            /* ...und Fokus daraufsetzen   */
         break;

      case ID_EDIT_SAVE:                    /* Sektor(en) abspeichern      */
         boReturn = ExportData(hwnd, ulDrive, ulSector, ulSecCount,
            ulEditMode);                    /* Sektoren aus MLE abspeich.  */

         if (boReturn == TRUE)              /* ...OK                       */
            DosBeep(2000, 100);

         break;

      case ID_INTP_HEX:                     /* Hex-Darstellung             */
      case ID_INTP_MBR:                     /* MBR_Interpretation          */
      case ID_INTP_PBR:                     /* Bootsektor-Interpretation   */
         SwitchWindows(ulWinMode = SHORT1FROMMP(mp1));

         break;

      case ID_HLP_ABOUT:                    /* Copyright ausgeben          */
         ShowMessage(MSG_HLPABOUT, 0);

         break;

      default:                              /* sonstige Kommandos          */
         WinDefWindowProc(hwnd, msg, mp1, mp2);
   }
} 


#pragma info (nogen)                        /* switch-Warnungen unterdr.   */

/***************************************************************************/
/*                                                                         */
/* Funktion: PartSelProc                                                   */
/*                                                                         */
/***************************************************************************/

static MRESULT EXPENTRY PartSelProc         /* Partitionsauswahldialog     */
(
   HWND hwnd,                               /* Fensterhandle               */
   USHORT msg,                              /* Message                     */
   MPARAM mp1,                              /* Messageparameter            */
   MPARAM mp2
)

{
   CHAR szDrive[3];                         /* Laufwerksstring             */

   HWND hwndDDL = WinWindowFromID(hwnd, ID_PARTSEL_SELECT);
                                            /* Fensterhdl. von DropDownL.  */


   switch (msg)                             /* welche Message?             */
   {
      case WM_INITDLG:                      /* Dialoginitialisierung       */
         if (ulEditMode == EDIT_ABS)        /* physikalischer Zugriff      */
            WinCheckButton(hwnd, ID_PARTSEL_PHYSPART, 1);
         else
            WinCheckButton(hwnd, ID_PARTSEL_LOGPART, 1);

         SetListBox(hwndDDL, ulEditMode, ulDrive);
                                            /* Listbox fÅllen              */
         break;

      case WM_COMMAND:                      /* Dialogkommando              */
         switch (SHORT1FROMMP(mp1))         
         {
            case ID_PARTSEL_OK:             /* OK-Taste gedrÅckt           */
               if (WinQueryWindowText(hwndDDL, sizeof (szDrive), szDrive)
                  > 1)                      /* Text in Eingabefeld         */
               {
                  if (ulEditMode == EDIT_ABS)
                                            /* phys. Partition auswÑhlen   */
                     ulDrive = ulPhysDrive = (CHAR)(szDrive[0] - '0');
                  else                      /* log. Partition auswÑhlen    */
                     ulDrive = ulLogDrive = (CHAR)(szDrive[0] - 'A' + 1);

                  WinPostMsg(hwndMain, WM_COMMAND, (MPARAM)ID_EDIT_LOAD,
                     MPVOID);
               }

            case ID_PARTSEL_CANCEL:         /* Abbruch-Taste gedrÅckt      */
               WinDismissDlg(hwnd, TRUE);   /* Dialogbox abbauen           */
               break;

            default:
               break;
         }
      
         break;

      case WM_CONTROL:                      /* Dialogkommando              */
         switch (SHORT1FROMMP(mp1))         
         {
            case ID_PARTSEL_PHYSPART:       /* physikalische Part. wÑhlen  */
               ulEditMode = EDIT_ABS;

               SetListBox(hwndDDL, ulEditMode, ulPhysDrive);
                                            /* Listbox fÅllen              */
               break;

            case ID_PARTSEL_LOGPART:        /* logische Partition wÑhlen   */
               ulEditMode = EDIT_LOG2;

               SetListBox(hwndDDL, ulEditMode, ulLogDrive);
                                            /* Listbox fÅllen              */
               break;

            default:
               break;
         }
      
         break;

      default:
         return WinDefDlgProc(hwnd, msg, mp1, mp2);
   }

   return (MRESULT)0; 
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: SecSelProc                                                    */
/*                                                                         */
/***************************************************************************/

static MRESULT EXPENTRY SecSelProc          /* Sektorauswahldialog         */
(
   HWND hwnd,                               /* Fensterhandle               */
   USHORT msg,                              /* Message                     */
   MPARAM mp1,                              /* Messageparameter            */
   MPARAM mp2
)

{
   CHAR szTemp[11];

   UINT uiCount = 0;

   HWND hwndStart = WinWindowFromID(hwnd, ID_SECSEL_STARTEDIT);
                                            /* Fensterhdl. von Startsek.   */
   HWND hwndCount = WinWindowFromID(hwnd, ID_SECSEL_COUNTEDIT);
                                            /* Fensterhdl. von Anzahl Sek. */


   switch (msg)                             /* welche Message?             */
   {
      case WM_INITDLG:                      /* Dialoginitialisierung       */
         /*---- Text in Eingabefeldern initialisieren ---------------------*/
         sprintf(szTemp, "%ld", ulSector);
         WinSetWindowText(hwndStart, szTemp);
         sprintf(szTemp, "%ld", ulSecCount);
         WinSetWindowText(hwndCount, szTemp);

         switch (ulEditMode)
         {
            case EDIT_ABS:                  /* physikalischer Zugriff      */
               WinEnableControl(hwnd, ID_SECSEL_RDWR, FALSE);
               WinEnableControl(hwnd, ID_SECSEL_IOCTL, FALSE);
 
               break;

            case EDIT_LOG1:                 /* logischer Zugriff mit IOCTL */
               WinCheckButton(hwnd, ID_SECSEL_IOCTL, 1);

               break;

            case EDIT_LOG2:                 /* logischer Zugriff mit RdWr  */
               WinCheckButton(hwnd, ID_SECSEL_RDWR, 1);

               break;
         }

         break;
      
      case WM_COMMAND:                      /* Dialogkommando              */
         switch (SHORT1FROMMP(mp1))         
         {
            case ID_SECSEL_OK:              /* OK-Taste gedrÅckt           */
               WinQueryWindowText(hwndStart, sizeof (szTemp), szTemp);
               uiCount += sscanf(szTemp, "%lu", &ulSector);
               WinQueryWindowText(hwndCount, sizeof (szTemp), szTemp);
               uiCount += sscanf(szTemp, "%lu", &ulSecCount);

               if (uiCount != 2)            /* Eingabe fehlerhaft          */
               {
                  ShowMessage(MSG_WRENTRY, MB_WARNING);
                  break;
               }
               else if (ulSecCount == 0 || ulSecCount > 16)
               {
                  ShowMessage(MSG_NUMSECS, MB_WARNING);
                  break;
               }
               else
                  WinPostMsg(hwndMain, WM_COMMAND, (MPARAM)ID_EDIT_LOAD,
                     MPVOID);

            case ID_SECSEL_CANCEL:          /* Abbruch-Taste gedrÅckt      */
               WinDismissDlg(hwnd, TRUE);   /* Dialogbox abbauen           */
               break;

            default:
               break;
         }
      
         break;

      case WM_CONTROL:                      /* Dialogkommando              */
         switch (SHORT1FROMMP(mp1))         
         {
            case ID_SECSEL_IOCTL:           /* IOCTL auswÑhlen             */
               ulEditMode = EDIT_LOG1;

               break;

            case ID_SECSEL_RDWR:            /* Read/Write auswÑhlen        */
               ulEditMode = EDIT_LOG2;

               break;

            default:
               break;
         }
      
         break;

      default:
         return WinDefDlgProc(hwnd, msg, mp1, mp2);
   }

   return (MRESULT)0; 
} 

#pragma info (restore)                      /* Warnungen wieder zulassen   */


/***************************************************************************/
/*                                                                         */
/* Funktion: ConvertProc                                                   */
/*                                                                         */
/***************************************************************************/

static MRESULT EXPENTRY ConvertProc         /* Sektoumrechnungsdialog      */
(
   HWND hwnd,                               /* Fensterhandle               */
   USHORT msg,                              /* Message                     */
   MPARAM mp1,                              /* Messageparameter            */
   MPARAM mp2
)

{
   static ULONG ulConvMode;                 /* Umrechnungsmodus            */
   static ULONG ulLogMode;                  /* bisheriger logischer Modus  */

   HWND hwndDDL = WinWindowFromID(hwnd, ID_CONV_SELECT);
                                            /* Fensterhdl. von DropDownL.  */

   switch (msg)                             /* welche Message?             */
   {
      case WM_INITDLG:                      /* Dialoginitialisierung       */
         ulConvMode = ulEditMode;
         ulLogMode = 
            (ULONG)(ulEditMode == EDIT_LOG1 ? EDIT_LOG1 : EDIT_LOG2);

         switch (ulConvMode)                /* welcher Umrechnungsmodus?   */
         {
            case EDIT_ABS:                  /* physikalischer Zugriff      */
               WinCheckButton(hwnd, ID_CONV_PHYSPART, 1);
               WinCheckButton(hwnd, ID_CONV_LOGPART, 0);

               WinEnableControl(hwnd, ID_CONV_RDWR, FALSE);
               WinEnableControl(hwnd, ID_CONV_IOCTL, FALSE);

               break;

            case EDIT_LOG1:                 /* log. Zugriff mit IOCTL      */
               WinCheckButton(hwnd, ID_CONV_LOGPART, 1);
               WinCheckButton(hwnd, ID_CONV_PHYSPART, 0);
               WinCheckButton(hwnd, ID_CONV_IOCTL, 1);
               WinCheckButton(hwnd, ID_CONV_RDWR, 0);

               break;

            case EDIT_LOG2:                 /* log. Zugriff mit Read/Write */
               WinCheckButton(hwnd, ID_CONV_LOGPART, 1);
               WinCheckButton(hwnd, ID_CONV_PHYSPART, 0);
               WinCheckButton(hwnd, ID_CONV_RDWR, 1);
               WinCheckButton(hwnd, ID_CONV_IOCTL, 0);

               break;
         }

         SetListBox(hwndDDL, ulConvMode, ulDrive);
                                            /* Listbox fÅllen              */
         break;

      case WM_COMMAND:                      /* Dialogkommando              */
         switch (SHORT1FROMMP(mp1))         
         {
            case ID_CONV_SEC2CHS:           /* Umrechnung Sektor ->ZKS     */
               ConvertSecToCHS(hwnd, ulConvMode);

               break;

            case ID_CONV_CHS2SEC:           /* Umrechnung ZKS -> Sektor    */
               ConvertCHSToSec(hwnd, ulConvMode);

               break;

            case ID_CONV_CANCEL:            /* Abbruch-Taste gedrÅckt      */
               WinDismissDlg(hwnd, TRUE);   /* Dialogbox abbauen           */
               break;

            default:
               break;
         }
      
         break;

      case WM_CONTROL:                      /* Dialogkommando              */
         switch (SHORT1FROMMP(mp1))         
         {
            case ID_CONV_LOGPART:
               if (ulConvMode == EDIT_ABS)  /* neuer Modus?                */
               {
                  WinCheckButton(hwnd, ID_CONV_LOGPART, 1);
                  WinCheckButton(hwnd, ID_CONV_PHYSPART, 0);

                  WinEnableControl(hwnd, ID_CONV_RDWR, TRUE);
                  WinEnableControl(hwnd, ID_CONV_IOCTL, TRUE);


                  ulConvMode = ulLogMode;
                  SetListBox(hwndDDL, ulConvMode, ulDrive);
                                            /* Listbox  neu fÅllen         */
               }

               break;

            case ID_CONV_PHYSPART:
               if (ulConvMode == EDIT_LOG1 || ulConvMode == EDIT_LOG2)
                                            /* neuer Modus?                */
               {
                  ulLogMode = ulConvMode;   /* retten fÅr spÑter           */
 
                  WinCheckButton(hwnd, ID_CONV_PHYSPART, 1);
                  WinCheckButton(hwnd, ID_CONV_LOGPART, 0);

                  WinEnableControl(hwnd, ID_CONV_RDWR, FALSE);
                  WinEnableControl(hwnd, ID_CONV_IOCTL, FALSE);

                  SetListBox(hwndDDL, ulConvMode = EDIT_ABS, ulPhysDrive);
                                            /* Listbox  neu fÅllen         */
               }

               break;

            case ID_CONV_IOCTL:
               if (ulConvMode == EDIT_LOG2) /* neuer Modus?                */
               {
                  WinCheckButton(hwnd, ID_CONV_IOCTL, 1);
                  WinCheckButton(hwnd, ID_CONV_RDWR, 0);

                  ulConvMode = ulLogMode = EDIT_LOG1;
               }

               break;

            case ID_CONV_RDWR:
               if (ulConvMode == EDIT_LOG1) /* neuer Modus?                */
               {
                  WinCheckButton(hwnd, ID_CONV_RDWR, 1);
                  WinCheckButton(hwnd, ID_CONV_IOCTL, 0);

                  ulConvMode = ulLogMode = EDIT_LOG2;
               }

               break;

            default:
               break;
         }
      
         break;

      default:
         return WinDefDlgProc(hwnd, msg, mp1, mp2);
   }

   return (MRESULT)0; 
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: ShowMessage                                                   */
/*                                                                         */
/***************************************************************************/

USHORT ShowMessage
(
   ULONG ulMessageId,                       /* welche Meldung              */
   ULONG ulFlags,                           /* Darstellungsmodi            */
   ...                                      /* optionale EinfÅgeparameter  */
)

{
   CHAR szText[100];                        /* Textpuffer                  */

   USHORT usRetCode;

   ULONG ulBoxFlags = 0;                    /* Flags der Messagebox        */

   va_list InsertParms;                     /* EinfÅgeparameter            */


   va_start(InsertParms, ulFlags);          /* variable Liste setzen       */


   /*---- Text zusammenbasteln --------------------------------------------*/
   vsprintf(szText, pszMsgArray[ulMessageId], InsertParms);

   /*---- Ausgabemodus ermitteln ------------------------------------------*/
   if (ulFlags & SM_WARNING)                /* Warnung ausgeben            */
      ulBoxFlags |= MB_WARNING;
   else
      ulBoxFlags |= MB_INFORMATION;
   if (ulFlags & SM_OKCANCEL)               /* Frage mit OK und Abbruch    */
      ulBoxFlags = MB_OKCANCEL | MB_QUERY;
   else
      ulBoxFlags |= MB_OK;

   
   usRetCode = (USHORT)WinMessageBox        /* Meldung ausgeben            */
   (
      HWND_DESKTOP,
      hwndMainFrame,
      szText,
      ulFlags & SM_WARNING ? NULL : "",
      0,
      ulBoxFlags | MB_MOVEABLE
   );

   return usRetCode;
}
 

/***************************************************************************/
/*                                                                         */
/* Funktion: QueryPartitions                                               */
/*                                                                         */
/***************************************************************************/

static BOOL QueryPartitions                 /* Partitionen erfragen        */
(
   VOID
)

{
   USHORT usPhysDriveNum;                   /* Parameter als USHORT        */


   /*---- Laufwerksinfos holen --------------------------------------------*/
   if (DosQueryCurrentDisk(&ulLogDrive, &ulLogDriveMap) != NO_ERROR)
      return FALSE;
   if (DosPhysicalDisk(INFO_COUNT_PARTITIONABLE_DISKS, &usPhysDriveNum,
      sizeof (usPhysDriveNum), NULL, 0) != NO_ERROR)
      return FALSE;


   ulPhysDriveNum = usPhysDriveNum;         /* Konvertierung in ULONG      */

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: SetListBox                                                    */
/*                                                                         */
/***************************************************************************/

static BOOL SetListBox
(
   HWND hwnd,                               /* Fensterhandle von Listbox   */
   ULONG ulFlags,                           /* phys. oder log. Partition?  */
   ULONG ulDefDrive                         /* Laufwerksvorgabe            */
)

{
   PSZ pszItem = " :";

   SHORT sCount = 0;

   UINT uiTmpDrive;


   WinSendMsg(hwnd, LM_DELETEALL, MPVOID, MPVOID);
                                            /* alle bish. EintrÑge lîschen */

   if (ulFlags == EDIT_ABS)                 /* phys. Partition auswÑhlen   */
   {
      for (uiTmpDrive = 1; uiTmpDrive <= ulPhysDriveNum; uiTmpDrive++)
      {
         pszItem[0] = (CHAR)(uiTmpDrive + '0');
         WinSendMsg(hwnd, LM_INSERTITEM, MPFROMSHORT(LIT_SORTASCENDING), 
            MPFROMP(pszItem));
      }

      WinSendMsg(hwnd, LM_SETTOPINDEX, (MPARAM)(ulDefDrive - 1), MPVOID);
      WinSendMsg(hwnd, LM_SELECTITEM, (MPARAM)(ulDefDrive - 1), 
         (MPARAM)TRUE);
   }
   else                                     /* log. Partition auswÑhlen    */
   {
      for (uiTmpDrive = 0; uiTmpDrive < 26; uiTmpDrive++)
         if ((ulLogDriveMap >> uiTmpDrive) & 0x00000001)
         {
            pszItem[0] = (CHAR)(uiTmpDrive + 'A');
            WinSendMsg(hwnd, LM_INSERTITEM,
               MPFROMSHORT(LIT_SORTASCENDING), MPFROMP(pszItem));

            if (uiTmpDrive < ulDefDrive - 1)
               sCount++;
         }

      WinSendMsg(hwnd, LM_SETTOPINDEX, MPFROMSHORT(sCount), MPVOID);
      WinSendMsg(hwnd, LM_SELECTITEM, MPFROMSHORT(sCount), (MPARAM)TRUE);
   }
 

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: SwitchWindows                                                 */
/*                                                                         */
/***************************************************************************/

static BOOL SwitchWindows
(
   ULONG ulWinID
)

{
   switch (ulWinID)                         /* welches Fenster?            */
   {
      case ID_INTP_HEX:                     /* Interpretation in Hex       */
         WinShowWindow(hwndMBR, FALSE);     /* MBR-Fenster deaktivieren    */
         WinShowWindow(hwndPBR, FALSE);     /* PBR-Fenster deaktivieren    */

         WinShowWindow(hwndEdit, TRUE);     /* MLE-Fenster aktivieren      */
         WinShowWindow(hwndButtonSave, TRUE);
         WinShowWindow(hwndButtonLoad, TRUE);

         WinSetFocus(HWND_DESKTOP, hwndEdit);
                                            /* ...und Fokus daraufsetzen   */

         break;

      case ID_INTP_MBR:                     /* Interpretation als MBR      */
         WinShowWindow(hwndEdit, FALSE);    /* MLE-Fenster deaktivieren    */
         WinShowWindow(hwndPBR, FALSE);     /* PBR-Fenster deaktivieren    */
         WinShowWindow(hwndButtonSave, FALSE);
         WinShowWindow(hwndButtonLoad, FALSE);

         WinShowWindow(hwndMBR, TRUE);      /* MBR_Fenster aktivieren      */

         WinSetFocus(HWND_DESKTOP, hwndMBR);
                                            /* ...und Fokus daraufsetzen   */
         break;

      case ID_INTP_PBR:                     /* Interpretation als PBR      */
         WinShowWindow(hwndEdit, FALSE);    /* MLE-Fenster deaktivieren    */
         WinShowWindow(hwndButtonSave, FALSE);
         WinShowWindow(hwndButtonLoad, FALSE);
         WinShowWindow(hwndMBR, FALSE);     /* MBR-Fenster deaktivieren    */

         WinShowWindow(hwndPBR, TRUE);      /* PBR-Fenster aktivieren      */

         WinSetFocus(HWND_DESKTOP, hwndPBR);
                                            /* ...und Fokus daraufsetzen   */

         break;
   }

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: ConvertSecToCHS                                               */
/*                                                                         */
/***************************************************************************/
static BOOL ConvertSecToCHS
(
   HWND hwnd,                               /* Fensterhandle               */
   ULONG ulConvMode                         /* Umrechnungsmodus            */
)

{
   CHAR szDrive[3];                         /* Laufwerksstring             */
   CHAR szTemp[11];

   UINT uiCount = 0;

   ULONG ulConvSec;                         /* Umrechnungssektor           */
   ULONG ulHiddenSecs;                      /* versteckte Sektoren         */

   GEOMETRY Location;                       /* position im ZKS-Format      */

   HWND hwndSec = WinWindowFromID(hwnd, ID_CONV_SRLEDIT);
                                            /* Fensterhandle des Sektors   */
   HWND hwndPart = WinWindowFromID(hwnd, ID_CONV_SELECT);
                                            /* Fensterhandle der Partition */
   HWND hwndCyl = WinWindowFromID(hwnd, ID_CONV_CYLEDIT);
                                            /* Fensterhandle des Zylinders */
   HWND hwndHead = WinWindowFromID(hwnd, ID_CONV_HEADEDIT);
                                            /* Fensterhandle des Kopfes    */
   HWND hwndSector = WinWindowFromID(hwnd, ID_CONV_SECEDIT);
                                            /* Fensterhandle des Sektors   */
   HWND hwndHidden = WinWindowFromID(hwnd, ID_CONV_HIDSECEDIT);
                                            /* Fensterhandle der verst. S. */

   APIRET RetCode;


   WinQueryWindowText(hwndSec, sizeof (szTemp), szTemp);
   uiCount += sscanf(szTemp, "%lu", &ulConvSec);
   WinQueryWindowText(hwndPart, sizeof (szDrive), szDrive);

   if (uiCount != 1)                        /* Eingabe fehlerhaft          */
   {
      ShowMessage(MSG_WRENTRY, MB_WARNING);
      return FALSE;
   }

   switch (ulConvMode)                      /* Umrechnungsmodus?           */
   {
      case EDIT_ABS:                        /* physikalische Partition     */
         RetCode = ABS_RBAToGeometry(szDrive[0], ulConvSec, &Location);

         WinSetWindowText(hwndHidden, "---");

         break;

      case EDIT_LOG1:                       /* log. Partition mit IOCTL    */
         RetCode = LOG1_RBAToGeometry(szDrive[0], ulConvSec, &Location,
            &ulHiddenSecs);

         sprintf(szTemp, "%lu", ulHiddenSecs);
         WinSetWindowText(hwndHidden, szTemp);

         break;

      case EDIT_LOG2:                       /* log. Part. mit Read/Write   */
         RetCode =LOG2_RBAToGeometry(szDrive[0], ulConvSec, &Location,
            &ulHiddenSecs);

         sprintf(szTemp, "%lu", ulHiddenSecs);
         WinSetWindowText(hwndHidden, szTemp);

         break;
   }

   if (RetCode != NO_ERROR)                 /* Fehler bei Umrechnung       */
      ShowMessage(MSG_CVSERR, SM_WARNING, RetCode);
                                            /* Warnung ausgeben            */
   else
   {
      sprintf(szTemp, "%lu", Location.ulCylinder);
      WinSetWindowText(hwndCyl, szTemp);
      sprintf(szTemp, "%lu", Location.ulHead);
      WinSetWindowText(hwndHead, szTemp);
      sprintf(szTemp, "%lu", Location.ulSector);
      WinSetWindowText(hwndSector, szTemp);
   }


   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: ConvertCHSToSec                                               */
/*                                                                         */
/***************************************************************************/
static BOOL ConvertCHSToSec
(
   HWND hwnd,                               /* Fensterhandle               */
   ULONG ulConvMode                         /* Umrechnungsmodus            */
)

{
   CHAR szDrive[3];                         /* Laufwerksstring             */
   CHAR szTemp[11];

   UINT uiCount = 0;

   ULONG ulConvSec;                         /* Umrechnungssektor           */
   ULONG ulHiddenSecs;                      /* versteckte Sektoren         */

   GEOMETRY Location;                       /* position im ZKS-Format      */

   HWND hwndSec = WinWindowFromID(hwnd, ID_CONV_SRLEDIT);
                                            /* Fensterhandle des Sektors   */
   HWND hwndPart = WinWindowFromID(hwnd, ID_CONV_SELECT);
                                            /* Fensterhandle der Partition */
   HWND hwndCyl = WinWindowFromID(hwnd, ID_CONV_CYLEDIT);
                                            /* Fensterhandle des Zylinders */
   HWND hwndHead = WinWindowFromID(hwnd, ID_CONV_HEADEDIT);
                                            /* Fensterhandle des Kopfes    */
   HWND hwndSector = WinWindowFromID(hwnd, ID_CONV_SECEDIT);
                                            /* Fensterhandle des Sektors   */
   HWND hwndHidden = WinWindowFromID(hwnd, ID_CONV_HIDSECEDIT);
                                            /* Fensterhandle der verst. S. */

   APIRET RetCode;


   WinQueryWindowText(hwndCyl, sizeof (szTemp), szTemp);
   uiCount += sscanf(szTemp, "%lu", &Location.ulCylinder);
   WinQueryWindowText(hwndHead, sizeof (szTemp), szTemp);
   uiCount += sscanf(szTemp, "%lu", &Location.ulHead);
   WinQueryWindowText(hwndSector, sizeof (szTemp), szTemp);
   uiCount += sscanf(szTemp, "%lu", &Location.ulSector);
   WinQueryWindowText(hwndPart, sizeof (szDrive), szDrive);

   if (uiCount != 3)                        /* Eingabe fehlerhaft          */
   {
      ShowMessage(MSG_WRENTRY, MB_WARNING);
      return FALSE;
   }

   switch (ulConvMode)                      /* Umrechnungsmodus?           */
   {
      case EDIT_ABS:                        /* physikalische Partition     */
         RetCode = ABS_GeometryToRBA(szDrive[0], &Location, &ulConvSec);

         WinSetWindowText(hwndHidden, "---");

         break;

      case EDIT_LOG1:                       /* log. Partition mit IOCTL    */
         RetCode = LOG1_GeometryToRBA(szDrive[0], &Location, &ulConvSec,
            &ulHiddenSecs);

         sprintf(szTemp, "%lu", ulHiddenSecs);
         WinSetWindowText(hwndHidden, szTemp);

         break;

      case EDIT_LOG2:                       /* log. Part. mit Read/Write   */
         RetCode = LOG2_GeometryToRBA(szDrive[0], &Location, &ulConvSec,
            &ulHiddenSecs);

         sprintf(szTemp, "%lu", ulHiddenSecs);
         WinSetWindowText(hwndHidden, szTemp);

         break;
   }

   if (RetCode != NO_ERROR)                 /* Fehler bei Umrechnung       */
      ShowMessage(MSG_CVSERR, SM_WARNING, RetCode);
                                            /* Warnung ausgeben            */
   else
   {
      sprintf(szTemp, "%lu", ulConvSec);
      WinSetWindowText(hwndSec, szTemp);
   }


   return TRUE;
}



