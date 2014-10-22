/***************************************************************************/
/*                                                                         */
/* Modul: RdWr8.C                                                          */
/* Autor: Michael Schmidt                                                  */
/* Datum: 11.12.1994                                                       */
/*                                                                         */
/***************************************************************************/

#define INCL_BASE
#define INCL_DOSDEVIOCTL

#include <os2.h>                            /* OS/2-Deklarationen          */

#include <stdlib.h>

#include "rdwr89.h"                         /* Konstanten                  */


/*---- Datentypen ---------------------------------------------------------*/
typedef struct _QUERYGEOMETRY               /* Struktur fr QUERYGEOMETRY  */
{
   BIOSPARAMETERBLOCK BiosParameterBlock;   /* extended BPB                */
   USHORT usCylinders;                      /* Anzahl Zylinder             */
   BYTE bDeviceType;                        /* Ger„tetyp                   */
   USHORT usDeviceAttributes;               /* Ger„teattribut              */
}
   QUERYGEOMETRY, *PQUERYGEOMETRY;

/*---- modulglobale Variablen ---------------------------------------------*/

/*---- Funktionsprototypen ------------------------------------------------*/
static APIRET Cat8_ReadSectors(HFILE hPartHandle, ULONG ulStartSector,
   ULONG ulSecCount, PPARTGEOMETRY pPartition, PVOID pvBuffer);
                                            /* Sektorgruppe lesen          */
static APIRET Cat8_WriteSectors(HFILE hPartHandle, ULONG ulStartSector,
   ULONG ulSecCount, PPARTGEOMETRY pPartition, PVOID pvBuffer);
                                            /* Sektorgruppe schreiben      */

static APIRET Cat8_GetGeometry(HFILE hPartHandle, PPARTGEOMETRY pPartition,
   PULONG pulHiddenSectors);                /* Partitionsgeometrie erfrag. */


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG1_ReadSectors                                              */
/*                                                                         */
/***************************************************************************/

APIRET LOG1_ReadSectors
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulSecIndex,                        /* Position in Partition       */
   ULONG ulSecCount,                        /* Anzahl zu lesender Sektoren */
   PVOID pvReadBuffer,                      /* Zeiger auf Lesepuffer       */
   BOOL boLock                              /* Partition sperren?          */
)

{
   BYTE bCommandInfo = 0, bDataInfo = 0;    /* fr DosDevIOCtl(...)        */
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   ULONG ulAction;                          /* Vorgang (nur zurck)        */
   ULONG ulParmLen;                         /* fr DosDevIOCtl(...)        */
   ULONG ulHiddenSectors;                   /* versteckte Sektoren         */

   HFILE hPartHandle;                       /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   if (ulSecCount == 0)                     /* keinen Sektor lesen         */
      return NO_ERROR;                      /* ...OK                       */

   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYWRITE |                /* kein anderer darf schreiben */
      OPEN_ACCESS_READONLY,                 /* wir wollen nur lesen        */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = Cat8_GetGeometry(hPartHandle, &Partition, &ulHiddenSectors);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = DosDevIOCtl                    /* Partition sperren           */
   (
      hPartHandle,                          /* Partitionshandle            */
      IOCTL_DISK,                           /* Kategorie Disk-Zugriff (8)  */
      DSK_LOCKDRIVE,                        /* Laufwerk sperren            */
      &bCommandInfo,                        /* muá 0 sein                  */
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen,                           /* Parameterl„nge (zurck)     */
      &bDataInfo,
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen                            /* Parameterl„nge (zurck)     */
   );

   if (RetCode != NO_ERROR && boLock == TRUE)
                                            /* Fehler bei Sperren          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = Cat8_ReadSectors(hPartHandle, ulSecIndex, ulSecCount, 
      &Partition, pvReadBuffer);            /* Sektorgruppe lesen          */

   DosDevIOCtl                              /* Partition freigeben         */
   (
      hPartHandle,                          /* Partitionshandle            */
      IOCTL_DISK,                           /* Kategorie Disk-Zugriff (8)  */
      DSK_UNLOCKDRIVE,                      /* Laufwerk freigeben          */
      &bCommandInfo,                        /* muá 0 sein                  */
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen,                           /* Parameterl„nge (zurck)     */
      &bDataInfo,
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen                            /* Parameterl„nge (zurck)     */
   );


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG1_WriteSectors                                             */
/*                                                                         */
/***************************************************************************/

APIRET LOG1_WriteSectors
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulSecIndex,                        /* Position in Partition       */
   ULONG ulSecCount,                        /* Anzahl zu schreib. Sektoren */
   PVOID pvWriteBuffer,                     /* Zeiger auf Schreibpuffer    */
   BOOL boLock                              /* Partition sperren?          */
)

{
   BYTE bCommandInfo = 0, bDataInfo = 0;    /* fr DosDevIOCtl(...)        */
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   ULONG ulAction;                          /* Vorgang (nur zurck)        */
   ULONG ulParmLen;                         /* fr DosDevIOCtl(...)        */
   ULONG ulHiddenSectors;                   /* versteckte Sektoren         */

   HFILE hPartHandle;                       /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   if (ulSecCount == 0)                     /* keinen Sektor schreiben     */
      return NO_ERROR;                      /* ...OK                       */

   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYREADWRITE |            /* kein anderer darf 'ran      */
      OPEN_ACCESS_WRITEONLY,                /* wir wollen nur schreiben    */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = Cat8_GetGeometry(hPartHandle, &Partition, &ulHiddenSectors);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = DosDevIOCtl                    /* Partition sperren           */
   (
      hPartHandle,                          /* Partitionshandle            */
      IOCTL_DISK,                           /* Kategorie Disk-Zugriff (8)  */
      DSK_LOCKDRIVE,                        /* Laufwerk sperren            */
      &bCommandInfo,                        /* muá 0 sein                  */
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen,                           /* Parameterl„nge (zurck)     */
      &bDataInfo,
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen                            /* Parameterl„nge (zurck)     */
   );

   if (RetCode != NO_ERROR && boLock == TRUE)
                                            /* Fehler bei Sperren          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = Cat8_WriteSectors(hPartHandle, ulSecIndex, ulSecCount,
      &Partition, pvWriteBuffer);           /* Sektorgruppe schreiben      */

   DosDevIOCtl                              /* Partition freigeben         */
   (
      hPartHandle,                          /* Partitionshandle            */
      IOCTL_DISK,                           /* Kategorie Disk-Zugriff (8)  */
      DSK_UNLOCKDRIVE,                      /* Laufwerk freigeben          */
      &bCommandInfo,                        /* muá 0 sein                  */
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen,                           /* Parameterl„nge (zurck)     */
      &bDataInfo,
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen                            /* Parameterl„nge (zurck)     */
   );


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */


   return RetCode;                          /* Rckgabewert bei Schreiben  */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG1_RBAToGeometry                                            */
/*                                                                         */
/***************************************************************************/

APIRET LOG1_RBAToGeometry
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulRBA,                             /* RBA (input)                 */
   PGEOMETRY pGeometry,                     /* ZKS-Struktur (output)       */
   PULONG pulHiddenSectors                  /* versteckte Sektoren (output)*/
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   ULONG ulAction;                          /* Vorgang (nur zurck)        */

   HFILE hPartHandle;                       /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYNONE |                 /* wir brauchen nur das Handle */
      OPEN_ACCESS_READONLY,                 /* wir wollen nur lesen        */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = Cat8_GetGeometry(hPartHandle, &Partition, pulHiddenSectors);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */

 
   RBAToGeometry(ulRBA, pGeometry, &Partition);
                                            /* Umrechnung in ZKS-Format    */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG1_GeometryToRBA                                            */
/*                                                                         */
/***************************************************************************/

APIRET LOG1_GeometryToRBA
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   PGEOMETRY pGeometry,                     /* ZKS-Struktur (input)        */
   PULONG pulRBA,                           /* RBA (output)                */
   PULONG pulHiddenSectors                  /* versteckte Sektoren (output)*/
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   ULONG ulAction;                          /* Vorgang (nur zurck)        */

   HFILE hPartHandle;                       /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYNONE |                 /* wir brauchen nur das Handle */
      OPEN_ACCESS_READONLY,                 /* wir wollen nur lesen        */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = Cat8_GetGeometry(hPartHandle, &Partition, pulHiddenSectors);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */

 
   *pulRBA = GeometryToRBA(pGeometry, &Partition);
                                            /* Umrechnung in ZKS-Format    */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG2_ReadBytes                                                */
/*                                                                         */
/***************************************************************************/

APIRET LOG2_ReadBytes
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulStartPos,                        /* Position in Partition       */
   ULONG ulSize,                            /* Anzahl zu lesender Bytes    */
   PVOID pvReadBuffer                       /* Zeiger auf Lesepuffer       */
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */
 
   ULONG ulAction;                          /* Vorgang (nur zurck)        */
   ULONG ulBytesRead;                       /* gelesene Bytes              */

   HFILE hPartHandle;                       /* Partitionshandle            */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */

   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYWRITE |                /* kein anderer darf schreiben */
      OPEN_ACCESS_READONLY,                 /* wir wollen nur lesen        */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = DosSetFilePtr                  /* Position in Part. setzen    */
   (
      hPartHandle,                          /* Partitionshandle            */
      (LONG)ulStartPos,                     /* Position in Partition       */
      FILE_BEGIN,                           /* ...vom Beginn aus           */
      &ulAction                             /* Dummy fr neue Position     */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Position setzen  */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }
 

   RetCode = DosRead                        /* Bytes lesen                 */
   (
      hPartHandle,                          /* Partitionshandle            */
      pvReadBuffer,                         /* Zeiger auf Lesepuffer       */
      ulSize,                               /* Anzahl Bytes zu lesen       */
      &ulBytesRead                          /* gelesene Bytes              */
   );


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG2_WriteBytes                                               */
/*                                                                         */
/***************************************************************************/

APIRET LOG2_WriteBytes
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulStartPos,                        /* Position in Partition       */
   ULONG ulSize,                            /* Anzahl zu schreibend. Bytes */
   PVOID pvWriteBuffer                      /* Zeiger auf Schreibpuffer    */
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */
 
   ULONG ulAction;                          /* Vorgang (nur zurck)        */
   ULONG ulBytesWritten;                    /* geschriebene Bytes          */

   HFILE hPartHandle;                       /* Partitionshandle            */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */

   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYREADWRITE |            /* kein anderer darf ran       */
      OPEN_ACCESS_WRITEONLY,                /* wir wollen nur schreiben    */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = DosSetFilePtr                  /* Position in Part. setzen    */
   (
      hPartHandle,                          /* Partitionshandle            */
      (LONG)ulStartPos,                     /* Position in Partition       */
      FILE_BEGIN,                           /* ...vom Beginn aus           */
      &ulAction                             /* Dummy fr neue Position     */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Position setzen  */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }
 

   RetCode = DosWrite                       /* Bytes schreiben             */
   (
      hPartHandle,                          /* Partitionshandle            */
      pvWriteBuffer,                        /* Zeiger auf Schreibpuffer    */
      ulSize,                               /* Anzahl Bytes zu schreiben   */
      &ulBytesWritten                       /* geschriebene Bytes          */
   );


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */


   return RetCode;                          /* Rckgabewert bei Schreiben  */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG2_RBAToGeometry                                            */
/*                                                                         */
/***************************************************************************/

APIRET LOG2_RBAToGeometry
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulRBA,                             /* RBA (input)                 */
   PGEOMETRY pGeometry,                     /* ZKS-Struktur (output)       */
   PULONG pulHiddenSectors                  /* versteckte Sektoren (output)*/
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   ULONG ulAction;                          /* Vorgang (nur zurck)        */
   HFILE hPartHandle;                       /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYNONE |                 /* wir brauchen nur das Handle */
      OPEN_ACCESS_READONLY,                 /* wir wollen nur lesen        */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = Cat8_GetGeometry(hPartHandle, &Partition, pulHiddenSectors);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */

 
   RBAToGeometry(ulRBA + *pulHiddenSectors, pGeometry, &Partition);
                                            /* Umrechnung in ZKS-Format    */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: LOG2_GeometryToRBA                                            */
/*                                                                         */
/***************************************************************************/

APIRET LOG2_GeometryToRBA
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   PGEOMETRY pGeometry,                     /* ZKS-Struktur (input)        */
   PULONG pulRBA,                           /* Sektor (output)             */
   PULONG pulHiddenSectors                  /* versteckte Sektoren (output)*/
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   ULONG ulAction;                          /* Vorgang (nur zurck)        */

   HFILE hPartHandle;                       /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosOpen                        /* Partition ”ffnen            */
   (
      pszPartition,                         /* Partitionsname              */
      &hPartHandle,                         /* Partitionshandle (zurck)   */
      &ulAction,                            /* Vorgang (zurck)            */
      0,                                    /* Dateigr”áe (nicht benutzt)  */
      0,                                    /* Dateiattribute (nicht ben.) */
      OPEN_ACTION_OPEN_IF_EXISTS,           /* Open Flag                   */
      OPEN_FLAGS_DASD |                     /* Direct Access Flag (wicht.) */
      OPEN_SHARE_DENYNONE |                 /* wir brauchen nur das Handle */
      OPEN_ACCESS_READONLY,                 /* wir wollen nur lesen        */
      NULL                                  /* keine EA's                  */
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = Cat8_GetGeometry(hPartHandle, &Partition, pulHiddenSectors);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosClose(hPartHandle);                /* ...wieder schlieáen         */
      return RetCode;
   }


   DosClose(hPartHandle);                   /* ...wieder schlieáen         */

 
   *pulRBA = GeometryToRBA(pGeometry, &Partition) - *pulHiddenSectors;
                                            /* Umrechnung in ZKS-Format    */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: Cat8_ReadSectors                                              */
/*                                                                         */
/***************************************************************************/

static APIRET Cat8_ReadSectors
(
   HFILE hPartHandle,                       /* Partitionshandle            */
   ULONG ulStartSector,                     /* Startsektor                 */
   ULONG ulSecCount,                        /* Anzahl Sektoren             */
   PPARTGEOMETRY pPartition,                /* Partitionsgeometrie         */
   PVOID pvReadBuffer                       /* Lesepuffer                  */
)

{
   PBYTE pbIndex = pvReadBuffer;            /* Pufferindex                 */

   UINT uiIndex;

   ULONG ulCylinder, ulHead;                /* Schleifenvariablen          */
   ULONG ulParmLen;                         /* fr DosDevIOCtl(...)        */

   GEOMETRY FirstLocation, LastLocation;    /* Start-/Endeposition         */

   PTRACKLAYOUT pTrackLayout;               /* DosDevIOCtl Parameter Block */

   APIRET RetCode;


   /*---- Spurlayout aufbauen ---------------------------------------------*/
   ulParmLen = sizeof (*pTrackLayout) + (pPartition->ulSecsTrack - 1) * 4;
   pTrackLayout = malloc(ulParmLen);
   if (pTrackLayout == NULL)                /* Speicherfehler              */
      return ERROR_NOT_ENOUGH_MEMORY;

   for (uiIndex = 0; uiIndex < pPartition->ulSecsTrack; uiIndex++)
   {
      pTrackLayout->TrackTable[uiIndex].usSectorNumber = 
         (USHORT)(uiIndex + 1);
      pTrackLayout->TrackTable[uiIndex].usSectorSize = SECTOR_SIZE;
   }


   /*---- Start- und Endeposition errechnen -------------------------------*/
   RBAToGeometry(ulStartSector, &FirstLocation, pPartition);
   RBAToGeometry(ulStartSector + ulSecCount - 1, &LastLocation, pPartition);


   for (ulCylinder = FirstLocation.ulCylinder;
      ulCylinder <= LastLocation.ulCylinder; ulCylinder++)
   {
      for (ulHead = 0; ulHead <= pPartition->ulHead; ulHead++)
      {
         pTrackLayout->bCommand = 1;
         pTrackLayout->usHead = (USHORT)ulHead;
         pTrackLayout->usCylinder = (USHORT)ulCylinder;
         pTrackLayout->usFirstSector = 0;
         pTrackLayout->cSectors = (USHORT)pPartition->ulSecsTrack;

         if (ulCylinder == FirstLocation.ulCylinder)
         {
            if (ulHead < FirstLocation.ulHead) 
               continue;

            if (ulHead == FirstLocation.ulHead)
            {
               pTrackLayout->usFirstSector = 
                  (USHORT)(FirstLocation.ulSector - 1);
               pTrackLayout->cSectors -= 
                  (USHORT)(FirstLocation.ulSector - 1);
            }
         }

         if (ulCylinder == LastLocation.ulCylinder)
         {
            if (ulHead > LastLocation.ulHead)
               continue;

            if (ulHead == LastLocation.ulHead)
               pTrackLayout->cSectors -= (pPartition->ulSecsTrack -
                  LastLocation.ulSector);
         }

 
         RetCode = DosDevIOCtl              /* Partition lesen             */
         (
            hPartHandle,                    /* Partitionshandle            */
            IOCTL_DISK,                     /* Kategorie Disk-Zugriff (8)  */
            DSK_READTRACK,                  /* Spur lesen                  */
            pTrackLayout,                   /* vorher gesetzt              */
            ulParmLen,                      /* Parameterl„nge              */
            &ulParmLen,                     /* Parameterl„nge (zurck)     */
            pbIndex,                        /* Lesepuffer                  */
            (ULONG)(pTrackLayout->cSectors * SECTOR_SIZE),
                                            /* Parameterl„nge              */
            &ulParmLen                      /* Parameterl„nge (zurck)     */
         );

         pbIndex += (ULONG)pTrackLayout->cSectors * SECTOR_SIZE;
                                            /* Lesepuffer neu indizieren   */
      }
   }

 
   free(pTrackLayout);                      /* Puffer wieder freigeben     */

   return RetCode;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: Cat8_WriteSectors                                             */
/*                                                                         */
/***************************************************************************/

static APIRET Cat8_WriteSectors
(
   HFILE hPartHandle,                       /* Partitionshandle            */
   ULONG ulStartSector,                     /* Startsektor                 */
   ULONG ulSecCount,                        /* Anzahl Sektoren             */
   PPARTGEOMETRY pPartition,                /* Partitionsgeometrie         */
   PVOID pvWriteBuffer                      /* Schreibpuffer               */
)

{
   PBYTE pbIndex = pvWriteBuffer;           /* Pufferindex                 */

   UINT uiIndex;

   ULONG ulCylinder, ulHead;                /* Schleifenvariablen          */
   ULONG ulParmLen;                         /* fr DosDevIOCtl(...)        */

   GEOMETRY FirstLocation, LastLocation;    /* Start-/Endeposition         */

   PTRACKLAYOUT pTrackLayout;               /* DosDevIOCtl Parameter Block */

   APIRET RetCode;


   /*---- Spurlayout aufbauen ---------------------------------------------*/
   ulParmLen = sizeof (*pTrackLayout) + (pPartition->ulSecsTrack - 1) * 4;
   pTrackLayout = malloc(ulParmLen);
   if (pTrackLayout == NULL)                /* Speicherfehler              */
      return ERROR_NOT_ENOUGH_MEMORY;

   for (uiIndex = 0; uiIndex < pPartition->ulSecsTrack; uiIndex++)
   {
      pTrackLayout->TrackTable[uiIndex].usSectorNumber = 
         (USHORT)(uiIndex + 1);
      pTrackLayout->TrackTable[uiIndex].usSectorSize = SECTOR_SIZE;
   }


   /*---- Start- und Endeposition errechnen -------------------------------*/
   RBAToGeometry(ulStartSector, &FirstLocation, pPartition);
   RBAToGeometry(ulStartSector + ulSecCount - 1, &LastLocation, pPartition);


   for (ulCylinder = FirstLocation.ulCylinder;
      ulCylinder <= LastLocation.ulCylinder; ulCylinder++)
   {
      for (ulHead = 0; ulHead <= pPartition->ulHead; ulHead++)
      {
         pTrackLayout->bCommand = 1;
         pTrackLayout->usHead = (USHORT)ulHead;
         pTrackLayout->usCylinder = (USHORT)ulCylinder;
         pTrackLayout->usFirstSector = 0;
         pTrackLayout->cSectors = (USHORT)pPartition->ulSecsTrack;

         if (ulCylinder == FirstLocation.ulCylinder)
         {
            if (ulHead < FirstLocation.ulHead) 
               continue;

            if (ulHead == FirstLocation.ulHead)
            {
               pTrackLayout->usFirstSector = 
                  (USHORT)(FirstLocation.ulSector - 1);
               pTrackLayout->cSectors -= 
                  (USHORT)(FirstLocation.ulSector - 1);
            }
         }

         if (ulCylinder == LastLocation.ulCylinder)
         {
            if (ulHead > LastLocation.ulHead)
               continue;

            if (ulHead == LastLocation.ulHead)
               pTrackLayout->cSectors -= (pPartition->ulSecsTrack -
                  LastLocation.ulSector);
         }

 
         RetCode = DosDevIOCtl              /* Partition schreiben         */
         (
            hPartHandle,                    /* Partitionshandle            */
            IOCTL_DISK,                     /* Kategorie Disk-Zugriff (8)  */
            DSK_WRITETRACK,                 /* Spur schreiben              */
            pTrackLayout,                   /* vorher gesetzt              */
            ulParmLen,                      /* Parameterl„nge              */
            &ulParmLen,                     /* Parameterl„nge (zurck)     */
            pbIndex,                        /* Lesepuffer                  */
            (ULONG)(pTrackLayout->cSectors * SECTOR_SIZE),
                                            /* Parameterl„nge              */
            &ulParmLen                      /* Parameterl„nge (zurck)     */
         );

         pbIndex += (ULONG)pTrackLayout->cSectors * SECTOR_SIZE;
                                            /* Schreibpuffer neu indiz.    */
      }
   }

 
   free(pTrackLayout);                      /* Puffer wieder freigeben     */

   return RetCode;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: Cat8_GetGeometry                                              */
/*                                                                         */
/***************************************************************************/

static APIRET Cat8_GetGeometry
(
   HFILE hPartHandle,                       /* Partitionshandle            */
   PPARTGEOMETRY pPartition,                /* Partitionsgeometrie         */
   PULONG pulHiddenSectors                  /* versteckte Sektoren         */
)

{
   BYTE bCommandInfo = 0;

   ULONG ulDataLen = sizeof(QUERYGEOMETRY); /* fr DosDevIOCtl(...)        */
   ULONG ulCommandLen = sizeof (BYTE);

   QUERYGEOMETRY QueryGeometry;             /* Struktur fr DosDevIOCtl    */

   APIRET RetCode;


   RetCode = DosDevIOCtl                    /* Geometrie erfragen          */
   (
      hPartHandle,                          /* Partitionshandle            */
      IOCTL_DISK,                           /* Kategorie Disk-Zugriff (8)  */
      DSK_GETDEVICEPARAMS,                  /* Geometrie erfragen          */
      &bCommandInfo,                        /* 0 --> BPB holen             */
      ulCommandLen,                         /* Parameterl„nge              */
      &ulCommandLen,                        /* Parameterl„nge (zurck)     */
      &QueryGeometry,                       /* Datenpaket                  */
      ulDataLen,                            /* Parameterl„nge              */
      &ulDataLen                            /* Parameterl„nge (zurck)     */
   );


   /*---- Geometriedaten bertragen ---------------------------------------*/
   pPartition->ulCylinder = QueryGeometry.BiosParameterBlock.cCylinders;
   pPartition->ulHead = QueryGeometry.BiosParameterBlock.cHeads;
   pPartition->ulSecsTrack = 
      QueryGeometry.BiosParameterBlock.usSectorsPerTrack;

   *pulHiddenSectors = QueryGeometry.BiosParameterBlock.cHiddenSectors;
 

   return RetCode;
}



                                           

