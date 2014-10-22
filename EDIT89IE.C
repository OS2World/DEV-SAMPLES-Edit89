/***************************************************************************/
/*                                                                         */
/* Modul: Edit89IE.C                                                       */
/* Autor: Michael Schmidt                                                  */
/* Datum: 17.12.1994                                                       */
/*                                                                         */
/***************************************************************************/

#define INCL_PM
#define INCL_BASE

#include <os2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "edit89.h"                         /* Konstanten                  */
#include "rdwr89.h"                         /* DLL-Deklarationen           */

/*---- Funktionsprototypen ------------------------------------------------*/
static BOOL FormatLine(PSZ pszDestLine, PSZ pszSrcLine, ULONG ulAddress);
static BOOL ReFormatLine(PSZ pszDestLine, PSZ pszSrcLine);
static BOOL ChkHex(PCHAR pchByte);
static BOOL FillMBRPage(HWND hwnd, HPS hps);
static BOOL FillPBRPage(HWND hwnd, HPS hps);
static VOID BiosToGeometry(BYTE bHead, USHORT usSecCyl, 
   PGEOMETRY pGeometry);

/*---- Datentypen ---------------------------------------------------------*/
#pragma pack (1)                            /* auf Bytegrenze packen       */

typedef struct _PARTENTRY                   /* Eintrag in Partitionstab.   */
{
   BYTE bPartStat;                          /* Partitionsstatus            */
   BYTE bFirstHead;                         /* Startkopf der Partition     */
   USHORT usFirstSecCyl;                    /* Startsektor/-Zylinder       */
   BYTE bPartType;                          /* Partitionstyp               */
   BYTE bLastHead;                          /* letzter Kopf der Partition  */
   USHORT usLastSecCyl;                     /* letzter Sektor/Zylinder     */
   ULONG ulStartDist;                       /* Entfernung des 1. Sektors   */
   ULONG ulSecCount;                        /* Anzahl Sektoren in Part.    */
}
   PARTENTRY, *PPARTENTRY;

typedef struct _PARTSEC                     /* Partitionstabelle           */
{
   BYTE bReserved[0x1BE];                   /* Startcode der phys. Platte  */
   PARTENTRY PartEntry[4];                  /* 4 PartitionseintrÑge        */
   USHORT usPartSecID;                      /* Partitionssektorkennung     */
}
   PARTSEC, *PPARTSEC;

typedef struct _BOOTSEC                     /* logischer Bootsektor        */
{
   BYTE bReserved1[3];                      /* Sprungbefehl                */
   CHAR pchOsVersion[8];                    /* Betriebssystemversion       */
   USHORT usBytesSec;                       /* Bytes pro Sektor            */
   BYTE bSecsCluster;                       /* Sektoren pro Cluster        */
   USHORT usResSectors;                     /* Anzahl reservierte Sektoren */
   BYTE bFatCount;                          /* Anzahl FAT's                */
   USHORT usDirEntries;                     /* Anzahl EintrÑge im Root     */
   USHORT usSecCount;                       /* Anzahl Sektoren in Partit.  */
   BYTE bMediaID;                           /* Media-Descriptor            */
   USHORT usSecsFat;                        /* Sektoren pro FAT            */
   USHORT usSecsTrack;                      /* Sektoren pro Spur           */
   USHORT usHeads;                          /* Anzahl Kîpfe                */
   USHORT usDistance;                       /* Entfernung vom Plattenanf.  */
   BYTE bReserved2[480];                    /* Boot-Code                   */
   USHORT usBootSecID;                      /* Bootsektorkennung           */
}
   BOOTSEC, *PBOOTSEC;

#pragma pack ()


/*---- globale Variablen --------------------------------------------------*/
extern PCHAR pchXferBuffer;                 /* Transferpuffer              */


/***************************************************************************/
/*                                                                         */
/* Funktion: InitWindows                                                   */
/*                                                                         */
/***************************************************************************/

BOOL InitWindows                            /* Fenster initialisieren      */
(
   HWND hwnd                                /* Fensterhandle               */
)

{
   HWND hwndEdit = WinWindowFromID(hwnd, ID_EDIT_HEX);
                                            /* Fensterhandle des MLE       */
   HWND hwndMBR = WinWindowFromID(hwnd, ID_EDIT_MBR);
                                            /* Fensterhandle des MBR-F.    */
   HWND hwndPBR = WinWindowFromID(hwnd, ID_EDIT_PBR);
                                            /* Fensterhandle des PBR-F.    */


   /*---- monospaced Font fÅr alle Fenster setzen -------------------------*/
   WinSetPresParam(hwndEdit, PP_FONTNAMESIZE, 21, "10.System Monospaced");
   WinSetPresParam(hwndMBR, PP_FONTNAMESIZE, 21, "10.System Monospaced");
   WinSetPresParam(hwndPBR, PP_FONTNAMESIZE, 21, "10.System Monospaced");


   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: PaintOvlWindows                                               */
/*                                                                         */
/***************************************************************************/

BOOL PaintOvlWindows                        /* Ovl-Fenster neu aufbauen    */
(
   HWND hwnd,                               /* Fensterhandle               */
   HPS hps,                                 /* Presentation Space Handle   */
   ULONG ulWinMode                          /* welches Fenster aktiv?      */
)

{
   switch (ulWinMode)                       /* welches Fenster aktiv?      */
   {
      case ID_INTP_MBR:                     /* MBR-Fenster                 */
         FillMBRPage(hwnd, hps);            /* ...ausmalen                 */
         break;

      case ID_INTP_PBR:                     /* PBR-Fenster                 */
         FillPBRPage(hwnd, hps);            /* ...ausmalen                 */
         break;
   }
 
   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: ImportData                                                    */
/*                                                                         */
/***************************************************************************/

BOOL ImportData                             /* Text in Fenster bringen     */
(
   HWND hwnd,                               /* Fensterhandle               */
   ULONG ulDrive,                           /* welches Laufwerk?           */
   ULONG ulStartSec,                        /* Startsektor                 */
   ULONG ulSecCount,                        /* Anzahl Sektoren             */
   ULONG ulEditMode                         /* Zugriffsmodus               */
)

{
   PCHAR pchMLE;                            /* Zeiger auf MLE-Puffer       */

   ULONG ulBufSize = ulSecCount * SECTOR_SIZE;
                                            /* benîtigte Puffergrî·e       */
   ULONG ulLenMLE;                          /* LÑnge MLE-Text              */
   ULONG ulReadIndex = 0, ulMLEIndex = 0;   /* laufende Leseindices        */
   ULONG ulOffsetBegin = 0;                 /* Text beginnt bei 0          */

   HWND hwndEdit = WinWindowFromID(hwnd, ID_EDIT_HEX);
                                            /* Fensterhandle des MLE       */

   APIRET RetCode;


   if (ulSecCount == 0 || ulSecCount > 16)  /* maximal 16 Sektoren erlaubt */
   {
      ShowMessage(MSG_NUMSECS, SM_WARNING); /* Warnung ausgeben            */
      return FALSE;
   }

   ulLenMLE = (ulBufSize / 0x10 + ulSecCount * 2) * 80;
                                            /* benîtigte LÑnge fÅr MLE     */

   /*---- benîtigte Puffer allokieren -------------------------------------*/
   pchXferBuffer = realloc(pchXferBuffer, ulBufSize);
                                            /* Lesepuffer vergrî·ern       */
   if (pchXferBuffer == NULL)               /* realloc-Fehler              */
      return FALSE;
   pchMLE = malloc(ulLenMLE);               /* Puffer fÅr MLE allokieren   */
   if (pchMLE == NULL)                      /* malloc-Fehler               */
   {
      realloc(pchXferBuffer, SECTOR_SIZE);  /* Lesepuffer rÅcksetzen       */
      return FALSE;
   }


   /*---- Sektor(en) einlesen ---------------------------------------------*/
   switch (ulEditMode)                      /* ...auf welche Art?          */
   {
      case EDIT_LOG1:                       /* logisch mit IOCTL           */
         RetCode = LOG1_ReadSectors((CHAR)(ulDrive - 1 + 'A'), ulStartSec, 
            ulSecCount, pchXferBuffer, FALSE);
         break;

      case EDIT_LOG2:                       /* logisch mit 'DosRead(...)'  */
         RetCode = LOG2_ReadBytes((CHAR)(ulDrive - 1 + 'A'), 
            ulStartSec * SECTOR_SIZE, ulBufSize, pchXferBuffer);
         break;

      case EDIT_ABS:                        /* physikalisch mit IOCTL      */
         RetCode = ABS_ReadSectors((CHAR)(ulDrive + '0'), ulStartSec,
            ulSecCount, pchXferBuffer, FALSE);
         break;
   }

   if (RetCode != NO_ERROR)                 /* Lesefehler                  */
   {
      ShowMessage(MSG_RDERR, SM_WARNING, RetCode);
                                            /* Warnung ausgeben            */

      free(pchMLE);                         /* Puffer freigeben            */
      realloc(pchXferBuffer, SECTOR_SIZE);                    

      return FALSE;
   }

 
   /*---- Nun wird der zu importierende Text in's MLE-Zeilenformat --------*/
   /*---- gebracht. -------------------------------------------------------*/
   for (ulReadIndex = 0; ulReadIndex < ulBufSize; ulReadIndex += 0x10,
      ulMLEIndex += 80)
   {
      if (ulReadIndex % 0x200 == 0)         /* erste Zeile pro Sektor      */
      {
         /*---- erstmal Leerzeile einfÅgen --------------------------------*/
         sprintf(pchMLE + ulMLEIndex, "%79s\n", " ");

         sprintf(pchMLE + ulMLEIndex, "Sektor %ld von %ld Sektor(en):",
            ulReadIndex / 0x200 + 1, ulBufSize / 0x200);

         ulMLEIndex += 80;
      }

      FormatLine(pchMLE + ulMLEIndex, pchXferBuffer + ulReadIndex,
         ulReadIndex);

      if (ulReadIndex % 0x200 == 0x1F0)     /* letzte Zeile pro Sektor     */
      {
         ulMLEIndex += 80;

         sprintf(pchMLE + ulMLEIndex, "%79s\n", " ");
      }
   }
   

   WinSendMsg(hwndEdit, MLM_DELETE, (MPARAM)0, (MPARAM)0xFFFF);
                                            /* bish. MLE-Inhalt lîschen    */

   WinSendMsg(hwndEdit, MLM_FORMAT, (MPARAM)MLFIE_NOTRANS, NULL);
                                            /* MLE-Zeilenformat festlegen  */

   WinSendMsg(hwndEdit, MLM_SETIMPORTEXPORT, MPFROMP((PBYTE)pchMLE),
      MPFROMLONG(ulLenMLE));                /* Import/Export festlegen     */

   WinSendMsg(hwndEdit, MLM_IMPORT, MPFROMP(&ulOffsetBegin),
      MPFROMLONG(ulLenMLE));                /* Daten importieren           */

   free(pchMLE);                            /* MLE-Puffer freigeben        */
   if (realloc(pchXferBuffer, SECTOR_SIZE) == NULL) 
      return FALSE;                         /* Lesepuffer rÅcksetzen       */

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: ExportData                                                    */
/*                                                                         */
/***************************************************************************/

BOOL ExportData                             /* Daten aus Fenstern export.  */
(
   HWND hwnd,                               /* Fensterhandle               */
   ULONG ulDrive,                           /* welches Laufwerk?           */
   ULONG ulStartSec,                        /* Startsektor                 */
   ULONG ulSecCount,                        /* Anzahl Sektoren             */
   ULONG ulEditMode                         /* Zugriffsmodus               */
)

{
   PCHAR pchMLE;                            /* Zeiger auf MLE-Puffer       */

   BOOL boReturn = TRUE;                    /* RÅckgabewert                */

   ULONG ulBufSize = ulSecCount * SECTOR_SIZE;
                                            /* benîtigte Puffergrî·e       */
   ULONG ulLenMLE;                          /* LÑnge MLE-Text              */
   ULONG ulWriteIndex = 0, ulMLEIndex = 0;  /* laufende Schreibindices     */
   ULONG ulOffsetBegin = 0;                 /* Text beginnt bei 0          */

   HWND hwndEdit = WinWindowFromID(hwnd, ID_EDIT_HEX);
                                            /* Fensterhandle des MLE       */

   APIRET RetCode;


   WinSendMsg(hwndEdit, MLM_FORMAT, (MPARAM)MLFIE_NOTRANS, NULL);
                                            /* MLE-Zeilenformat festlegen  */

   ulLenMLE = (ULONG)WinSendMsg(hwndEdit, MLM_QUERYFORMATTEXTLENGTH, 0,
      (MPARAM)-1);                          /* TextlÑnge erfragen          */

   /*---- benîtigte Puffer allokieren -------------------------------------*/
   pchXferBuffer = realloc(pchXferBuffer, ulBufSize);      
                                            /* Schreibpuffer vergrî·ern    */
   if (pchXferBuffer == NULL)               /* realloc-Fehler              */
      return FALSE;
   pchMLE = malloc(ulLenMLE);               /* MLE-Puffer allokieren       */
   if (pchMLE == NULL)
   {
      realloc(pchXferBuffer, SECTOR_SIZE);  /* Schreibpuffer rÅcksetzen    */
      return FALSE;
   }

   WinSendMsg(hwndEdit, MLM_SETIMPORTEXPORT, MPFROMP((PBYTE)pchMLE),
      MPFROMLONG(ulLenMLE));                /* Import/Export festlegen     */

   ulLenMLE = (ULONG)WinSendMsg(hwndEdit, MLM_EXPORT,
      MPFROMP(&ulOffsetBegin), MPFROMLONG(&ulLenMLE));
                                            /* Daten exportieren           */


   /*---- Nun wird der zu exportierende Text in's BinÑrformat -------------*/
   /*---- gebracht. -------------------------------------------------------*/
   for (ulMLEIndex = 0; ulMLEIndex < ulLenMLE - 80; ulMLEIndex += 80,
      ulWriteIndex += 0x10)
   {
      if (ulMLEIndex % 2720 == 2640)        /* letzte Zeile pro Sektor     */
         ulMLEIndex += 80;                  /* ...Åberspringen             */
      if (ulMLEIndex % 2720 == 0)           /* erste Zeile pro Sektor      */
         ulMLEIndex += 80;                  /* ...Åberspringen             */

      if (ReFormatLine(pchXferBuffer + ulWriteIndex, pchMLE + ulMLEIndex)
         == FALSE)                          /* MLE-Eingabefeld inkonsist.  */
      {
         ShowMessage(MSG_INCONS, SM_WARNING);
                                            /* Warnung ausgeben            */

         free(pchMLE);                      /* Puffer freigeben            */
         realloc(pchXferBuffer, SECTOR_SIZE);

         return FALSE;
      }
   }


   if (ShowMessage(MSG_CONFIRM, SM_OKCANCEL) == MBID_OK)
                                            /* Abfrage, ob wirklich        */
   {
      /*---- Sektor(en) 'rausschreiben ------------------------------------*/
      switch (ulEditMode)                   /* ...auf welche Art?          */
      {
         case EDIT_LOG1:                    /* logisch mit IOCTL           */
            RetCode = LOG1_WriteSectors((CHAR)(ulDrive - 1 + 'A'), ulStartSec,
               ulSecCount, pchXferBuffer, TRUE);
            break;

         case EDIT_LOG2:                    /* logisch mit 'DosRead(...)'  */
            RetCode = LOG2_WriteBytes((CHAR)(ulDrive - 1 + 'A'), 
               ulStartSec * SECTOR_SIZE, ulBufSize, pchXferBuffer);
            break;

         case EDIT_ABS:                     /* physikalisch mit IOCTL      */
            RetCode = ABS_WriteSectors((CHAR)(ulDrive + '0'), ulStartSec,
               ulSecCount, pchXferBuffer, TRUE);
            break;
      }

      if (RetCode != NO_ERROR)              /* Schreibfehler               */
      {
         ShowMessage(MSG_WRERR, SM_WARNING, RetCode);
                                            /* Warnung ausgeben            */

         boReturn = FALSE;
      }
   }
   else boReturn = FALSE;

 
   free(pchMLE);                            /* MLE-Puffer freigeben        */
   if (realloc(pchXferBuffer, SECTOR_SIZE) == NULL)
      return FALSE;                         /* Schreibpuffer rÅcksetzen    */

   return boReturn;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: FormatLine                                                    */
/*                                                                         */
/***************************************************************************/

static BOOL FormatLine
(
   PSZ pszDestLine,                         /* MLE-formatierte Zeile       */
   PSZ pszSrcLine,                          /* ursprÅngliche Zeile         */
   ULONG ulAddress                          /* relative Adresse            */
)

{
   PSZ pszIndex = pszSrcLine;

   UINT uiIndex;


   sprintf
   (
      pszDestLine,
      "%08X:  "
      "%02X %02X %02X %02X %02X %02X %02X %02X - "
      "%02X %02X %02X %02X %02X %02X %02X %02X    ",
      ulAddress,
      *(pszIndex++), *(pszIndex++), *(pszIndex++), *(pszIndex++),
      *(pszIndex++), *(pszIndex++), *(pszIndex++), *(pszIndex++),
      *(pszIndex++), *(pszIndex++), *(pszIndex++), *(pszIndex++),
      *(pszIndex++), *(pszIndex++), *(pszIndex++), *(pszIndex++)
   );

   pszDestLine += 63;
   pszIndex = pszSrcLine;

   for (uiIndex = 0; uiIndex < 0x10; uiIndex++)
   {
      CHAR chSign = pszSrcLine[uiIndex];

      if (isprint(chSign))                  /* druckbares Zeichen          */
         *(pszDestLine++) = chSign;
      else                                  /* nicht druckbar              */
         *(pszDestLine++) = '.';            /* Platzhalter                 */
   }

   *(pszDestLine) = '\n';                   /* neue Zeile                  */

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: ReFormatLine                                                  */
/*                                                                         */
/***************************************************************************/

static BOOL ReFormatLine
(
   PSZ pszDestLine,                         /* binÑre Zeile                */
   PSZ pszSrcLine                           /* MLE-formatierte Zeile       */
)

{
   UINT uiIndex;
   ULONG ulValue;


   for (uiIndex = 11; uiIndex < 35; uiIndex += 3)
   {
      if (ChkHex(pszSrcLine + uiIndex) == FALSE)
         return FALSE;                      /* keine Hexzahl               */

      sscanf(pszSrcLine + uiIndex, "%2X", &ulValue);
      *(pszDestLine++) = (CHAR)ulValue;
   }
   for (uiIndex = 37; uiIndex < 61; uiIndex += 3)
   {
      if (ChkHex(pszSrcLine + uiIndex) == FALSE)
         return FALSE;                      /* keine Hexzahl               */

      sscanf(pszSrcLine + uiIndex, "%2X", &ulValue);
      *(pszDestLine++) = (CHAR)ulValue;
   }

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: ChkHex                                                        */
/*                                                                         */
/***************************************************************************/

static BOOL ChkHex
(
   PCHAR pchByte                            /* Zeiger auf zu test. Bytes   */
)

{
   BYTE bChk = *(pchByte++);                /* erster Charakter            */

   if (bChk > 'f') return FALSE;
   if (bChk < 'a')
   {
      if (bChk > 'F') return FALSE;
      if (bChk < 'A')
      {
         if (bChk > '9') return FALSE;
         if (bChk < '0') return FALSE;
      }
   }

   bChk = *pchByte;                         /* zweiter Charakter           */

   if (bChk > 'f') return FALSE;
   if (bChk < 'a')
   {
      if (bChk > 'F') return FALSE;
      if (bChk < 'A')
      {
         if (bChk > '9') return FALSE;
         if (bChk < '0') return FALSE;
      }
   }

   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: FillMBRPage                                                   */
/*                                                                         */
/***************************************************************************/
static BOOL FillMBRPage
(
   HWND hwnd,                               /* Fensterhandle               */
   HPS hps                                  /* Presentation Space Handle   */
)

{
   CHAR szDrawLine[81];                     /* Zeilenpuffer                */
   PSZ pszHeadLine = "Partitionstabelle:";
   PSZ pszTable1 = "                   Start-              Ende-";
   PSZ pszTable2 = " Strt  FSTyp   Zyl. Kopf Sektor   Zyl. Kopf Sektor"
      "   Anz. Sekt.      Offset";

   UINT uiIndex;

   PPARTSEC pPartSec = (PPARTSEC)pchXferBuffer;

   RECTL rc, rcDraw;                        /* Rechteckkoordinaten         */
          

   WinQueryWindowRect(hwnd, &rc);           /* Fensterkoordinaten erfragen */


   rcDraw = rc;
   rcDraw.xLeft = 220;
   rcDraw.yBottom = rc.yTop - 40;
   WinDrawText(hps, -1, pszHeadLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS | DT_UNDERSCORE);

   rcDraw.xLeft = 0;
   rcDraw.yBottom -= 30;
   WinDrawText(hps, -1, pszTable1, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);
   rcDraw.yBottom -= 15;
   WinDrawText(hps, -1, pszTable2, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 25;

   for (uiIndex = 0; uiIndex < 4; uiIndex++)
   {
      CHAR chStart;
      PSZ pszType;
      GEOMETRY FirstPos, LastPos;

      PPARTENTRY pPartEntry = &(pPartSec->PartEntry[uiIndex]);

      BiosToGeometry(pPartEntry->bFirstHead, pPartEntry->usFirstSecCyl,
         &FirstPos);
      BiosToGeometry(pPartEntry->bLastHead, pPartEntry->usLastSecCyl,
         &LastPos);

      switch (pPartEntry->bPartStat)        /* startbare Partition?        */
      {
         case 0x00:                         /* nein                        */
            chStart = 'n';
            break;
         case 0x80:                         /* ja                          */
            chStart = 'j';
            break;
         default:                           /* unbestimmt                  */
            chStart = '?';
            break;
      }

      switch (pPartEntry->bPartType)        /* welcher Partitionstyp       */
      {
         case 0x00:                         /* unbenutzt                   */
            pszType = "UNUSED";
            break;
         case 0x01:                         /* DOS 12-Bit-FAT              */
            pszType = "DOS12";
            break;
         case 0x02:                         /* XENIX                       */
         case 0x03:
            pszType = "XENIX";
            break;
         case 0x04:                         /* DOS 16-Bit-FAT              */
            pszType = "DOS16";
            break;
         case 0x05:                         /* erweiterte Partition        */
            pszType = "EXTEND";
            break;
         case 0x06:                         /* BigDOS                      */
            pszType = "BIGDOS";
            break;
         case 0x07:                         /* installierbares Dateisystem */
            pszType = "IFS";
            break;
         case 0x08:                         /* verschiedene                */
            pszType = "SEVRAL";
            break;
         case 0x09:                         /* AIX                         */
            pszType = "AIX";
            break;
         case 0x0A:                         /* Boot Manager                */
            pszType = "BOTMGR";
            break;
         case 0x11:                         /* "schlafendes" DOS           */
         case 0x14:
         case 0x16:
         case 0x17:
            pszType = "DOSSLP";
            break;
         case 0x64:                         /* NOVELL Netware              */
         case 0x65:
         case 0x66:
         case 0x67:
         case 0x68:
         case 0x69:
            pszType = "NOVELL";
            break;
         case 0xD8:                         /* CP/M                        */
            pszType = "CPM";
            break;
         default:                           /* unbekannt                   */
            pszType = "UNKNWN";
            break;
      }


      sprintf(szDrawLine, "  %c   %6s  %5u  %3u  %3u    %5u  %3u  %3u"
         "     %10u  %10u",
         chStart, pszType,
         FirstPos.ulCylinder, FirstPos.ulHead, FirstPos.ulSector,
         LastPos.ulCylinder, LastPos.ulHead, LastPos.ulSector,
         pPartEntry->ulSecCount, pPartEntry->ulStartDist);

      WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
         DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);
      
      rcDraw.yBottom -= 15;
   }
 
   rcDraw.yBottom -= 25;

   sprintf(szDrawLine, " MBR-Kennung (55AA Hex) %sgÅltig!",
      pPartSec->usPartSecID == 0xAA55 ? "" : "un");

   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);


   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: FillPBRPage                                                   */
/*                                                                         */
/***************************************************************************/
static BOOL FillPBRPage
(
   HWND hwnd,                               /* Fensterhandle               */
   HPS hps                                  /* Presentation Space Handle   */
)

{
   CHAR szDrawLine[81];                     /* Zeilenpuffer                */
   PSZ pszHeadLine = "Bootsektor der logischen Partition:";

   PBOOTSEC pBootSec = (PBOOTSEC)pchXferBuffer;

   RECTL rc, rcDraw;                        /* Rechteckkoordinaten         */
          

   WinQueryWindowRect(hwnd, &rc);           /* Fensterkoordinaten erfragen */


   rcDraw = rc;
   rcDraw.xLeft = 150;
   rcDraw.yBottom = rc.yTop - 40;
   WinDrawText(hps, -1, pszHeadLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS | DT_UNDERSCORE);

   rcDraw.xLeft = 0;
   rcDraw.yBottom -= 30;
   sprintf(szDrawLine, "                                Betriebssystem ID:"
      "  %8.8s", pBootSec->pchOsVersion);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                                 Bytes pro Sektor:"
      "  %u", pBootSec->usBytesSec);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                             Sektoren pro Cluster:"
      "  %u", pBootSec->bSecsCluster);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                             reservierte Sektoren:"
      "  %u", pBootSec->usResSectors);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                                     Anzahl FAT's:"
      "  %u", pBootSec->bFatCount);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "     maximale Anzahl EintrÑge im Hauptverzeichnis:"
      "  %u", pBootSec->usDirEntries);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                     Anzahl Sektoren in Partition:"
      "  %u", pBootSec->usSecCount);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                                 Media Descriptor:"
      "  %02X (Hex)", pBootSec->bMediaID);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                                 Sektoren pro FAT:"
      "  %u", pBootSec->usSecsFat);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                                Sektoren pro Spur:"
      "  %u", pBootSec->usSecsTrack);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                                     Anzahl Kîpfe:"
      "  %u", pBootSec->usHeads);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);

   rcDraw.yBottom -= 15;
   sprintf(szDrawLine, "                              versteckte Sektoren:"
      "  %u", pBootSec->usDistance);
   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);



   rcDraw.yBottom -= 40;

   sprintf(szDrawLine, " Bootsektorkennung (55AA Hex) %sgÅltig!",
      pBootSec->usBootSecID == 0xAA55 ? "" : "un");

   WinDrawText(hps, -1, szDrawLine, &rcDraw, 0, 0,
      DT_BOTTOM | DT_LEFT | DT_TEXTATTRS);


   return TRUE;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: BiosToGeometry                                                */
/*                                                                         */
/***************************************************************************/

static VOID BiosToGeometry
(
   BYTE bHead,                              /* Kopf (input)                */
   USHORT usSecCyl,                         /* Sektor/Zylinder (input)     */
   PGEOMETRY pGeometry                      /* ZKS-Struktur (output)       */
)

{
   pGeometry->ulCylinder = 
      (ULONG)(usSecCyl >> 8) | (ULONG)((usSecCyl & 0x00C0) << 2);
   pGeometry->ulHead = bHead;
   pGeometry->ulSector = (ULONG)(usSecCyl & 0x003F);
}



