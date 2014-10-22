/***************************************************************************/
/*                                                                         */
/* Modul: RdWr9.C                                                          */
/* Autor: Michael Schmidt                                                  */
/* Datum: 17.12.1994                                                       */
/*                                                                         */
/***************************************************************************/

#define INCL_BASE
#define INCL_DOSDEVIOCTL

#include <os2.h>                            /* OS/2-Deklarationen          */

#include <stdlib.h>

#include "rdwr89.h"                         /* Konstanten                  */


/*---- Datentypen ---------------------------------------------------------*/
#pragma pack (2)                            /* auf 2-Byte-Grenze packen    */

typedef struct _QUERYGEOMETRY               /* Struktur fr QUERYGEOMETRY  */
{
   USHORT usReserved1;                      /* reserviertes Feld           */
   USHORT usCylinder;                       /* Zylinder                    */
   USHORT usHead;                           /* Kopf                        */
   USHORT usSecsTrack;                      /* Sektoren pro Spur           */
   USHORT usReserved2[4];                   /* reservierte Felder          */
}
   QUERYGEOMETRY, *PQUERYGEOMETRY;

#pragma pack ()

/*---- modulglobale Variablen ---------------------------------------------*/

/*---- Funktionsprototypen ------------------------------------------------*/
static APIRET Cat9_ReadSectors(HFILE hPartHandle, ULONG ulStartSector,
   ULONG ulSecCount, PPARTGEOMETRY pPartition, PVOID pvBuffer);
                                            /* Sektorgruppe lesen          */
static APIRET Cat9_WriteSectors(HFILE hPartHandle, ULONG ulStartSector,
   ULONG ulSecCount, PPARTGEOMETRY pPartition, PVOID pvBuffer);
                                            /* Sektorgruppe schreiben      */

static APIRET GetGeometry(HFILE hPartHandle, PPARTGEOMETRY pPartition);
                                            /* Partitionsgeometrie erfrag. */


/***************************************************************************/
/*                                                                         */
/* Funktion: ABS_ReadSectors                                               */
/*                                                                         */
/***************************************************************************/

APIRET ABS_ReadSectors
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

   SHANDLE shPartHandle;                    /* Partitionshandle            */

   ULONG ulParmLen;                         /* fr DosDevIOCtl(...)        */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   if (ulSecCount == 0)                     /* keinen Sektor lesen         */
      return NO_ERROR;                      /* ...OK                       */

   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosPhysicalDisk                /* Laufwerkshandle holen       */
   (
      INFO_GETIOCTLHANDLE,
      &shPartHandle,                        /* Partitionshandle (zurck)   */
      sizeof (shPartHandle),
      pszPartition,                         /* physikalische Partition     */
      3
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = GetGeometry(shPartHandle, &Partition);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
         sizeof (shPartHandle));            /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = DosDevIOCtl                    /* Partition sperren           */
   (
      shPartHandle,                         /* Partitionshandle            */
      IOCTL_PHYSICALDISK,                   /* Kategorie Disk-Zugriff (9)  */
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
      DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
         sizeof (shPartHandle));            /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = Cat9_ReadSectors(shPartHandle, ulSecIndex, ulSecCount,
      &Partition, pvReadBuffer);            /* Sektorgruppe lesen          */

   DosDevIOCtl                              /* Partition freigeben         */
   (
      shPartHandle,                         /* Partitionshandle            */
      IOCTL_PHYSICALDISK,                   /* Kategorie Disk-Zugriff (9)  */
      DSK_UNLOCKDRIVE,                      /* Laufwerk freigeben          */
      &bCommandInfo,                        /* muá 0 sein                  */
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen,                           /* Parameterl„nge (zurck)     */
      &bDataInfo,
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen                            /* Parameterl„nge (zurck)     */
   );


   DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
      sizeof (shPartHandle));               /* ...wieder schlieáen         */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: ABS_WriteSectors                                              */
/*                                                                         */
/***************************************************************************/

APIRET ABS_WriteSectors
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

   SHANDLE shPartHandle;                    /* Partitionshandle            */

   ULONG ulParmLen;                         /* fr DosDevIOCtl(...)        */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   if (ulSecCount == 0)                     /* keinen Sektor lesen         */
      return NO_ERROR;                      /* ...OK                       */

   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosPhysicalDisk                /* Laufwerkshandle holen       */
   (
      INFO_GETIOCTLHANDLE,
      &shPartHandle,                        /* Partitionshandle (zurck)   */
      sizeof (shPartHandle),
      pszPartition,                         /* physikalische Partition     */
      3
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = GetGeometry(shPartHandle, &Partition);
                                            /* Partitionsgeometrie erfrag. */
   if (RetCode != NO_ERROR)                 /* Fehler aufgetreten          */
   {
      DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
         sizeof (shPartHandle));            /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = DosDevIOCtl                    /* Partition sperren           */
   (
      shPartHandle,                         /* Partitionshandle            */
      IOCTL_PHYSICALDISK,                   /* Kategorie Disk-Zugriff (9)  */
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
      DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
         sizeof (shPartHandle));            /* ...wieder schlieáen         */
      return RetCode;
   }


   RetCode = Cat9_WriteSectors(shPartHandle, ulSecIndex, ulSecCount,
      &Partition, pvWriteBuffer);           /* Sektorgruppe schreiben      */

   DosDevIOCtl                              /* Partition freigeben         */
   (
      shPartHandle,                         /* Partitionshandle            */
      IOCTL_PHYSICALDISK,                   /* Kategorie Disk-Zugriff (9)  */
      DSK_UNLOCKDRIVE,                      /* Laufwerk freigeben          */
      &bCommandInfo,                        /* muá 0 sein                  */
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen,                           /* Parameterl„nge (zurck)     */
      &bDataInfo,
      sizeof (BYTE),                        /* Parameterl„nge              */
      &ulParmLen                            /* Parameterl„nge (zurck)     */
   );


   DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
      sizeof (shPartHandle));               /* ...wieder schlieáen         */


   return RetCode;                          /* Rckgabewert bei Lesen      */
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: ABS_RBAToGeometry                                             */
/*                                                                         */
/***************************************************************************/

APIRET ABS_RBAToGeometry
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   ULONG ulRBA,                             /* Sektoradresse               */
   PGEOMETRY pGeometry                      /* ZKS-Struktur                */
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   SHANDLE shPartHandle;                    /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosPhysicalDisk                /* Laufwerkshandle holen       */
   (
      INFO_GETIOCTLHANDLE,
      &shPartHandle,                        /* Partitionshandle (zurck)   */
      sizeof (shPartHandle),
      pszPartition,                         /* physikalische Partition     */
      3
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = GetGeometry(shPartHandle, &Partition);
                                            /* Partitionsgeometrie erfrag. */

   DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
      sizeof (shPartHandle));               /* ...wieder schlieáen         */


   RBAToGeometry(ulRBA, pGeometry, &Partition);
                                            /* Umrechnung ins ZKS-Format   */


   return RetCode;                         
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: ABS_GeometryToRBA                                             */
/*                                                                         */
/***************************************************************************/

APIRET ABS_GeometryToRBA
(
   CHAR chDrive,                            /* Laufwerksbuchstabe          */
   PGEOMETRY pGeometry,                     /* ZKS-Struktur                */
   PULONG pulRBA                            /* Sektoradresse               */
)

{
   PSZ pszPartition = " :";                 /* Partitionsstring            */

   SHANDLE shPartHandle;                    /* Partitionshandle            */

   PARTGEOMETRY Partition;                  /* Partitionsgeometrie         */

   APIRET RetCode;


   pszPartition[0] = chDrive;               /* Partitionsstring aufbauen   */


   RetCode = DosPhysicalDisk                /* Laufwerkshandle holen       */
   (
      INFO_GETIOCTLHANDLE,
      &shPartHandle,                        /* Partitionshandle (zurck)   */
      sizeof (shPartHandle),
      pszPartition,                         /* physikalische Partition     */
      3
   );

   if (RetCode != NO_ERROR)                 /* Fehler bei Open             */
      return RetCode;


   RetCode = GetGeometry(shPartHandle, &Partition);
                                            /* Partitionsgeometrie erfrag. */

   DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &shPartHandle,
      sizeof (shPartHandle));               /* ...wieder schlieáen         */


   *pulRBA = GeometryToRBA(pGeometry, &Partition);
                                            /* Umrechnung ins RBA-Format   */


   return RetCode;                         
} 


/***************************************************************************/
/*                                                                         */
/* Funktion: Cat9_ReadSectors                                              */
/*                                                                         */
/***************************************************************************/

static APIRET Cat9_ReadSectors
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
            IOCTL_PHYSICALDISK,             /* Kategorie Disk-Zugriff (9)  */
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
                                            /* Schreibpuffer neu indiz.    */
      }
   }

 
   free(pTrackLayout);                      /* Puffer wieder freigeben     */

   return RetCode;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: Cat9_WriteSectors                                             */
/*                                                                         */
/***************************************************************************/

static APIRET Cat9_WriteSectors
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
            IOCTL_PHYSICALDISK,             /* Kategorie Disk-Zugriff (9)  */
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
/* Funktion: GetGeometry                                                   */
/*                                                                         */
/***************************************************************************/

static APIRET GetGeometry
(
   HFILE hPartHandle,                       /* Partitionshandle            */
   PPARTGEOMETRY pPartition                 /* Partitionsgeometrie         */
)

{
   BYTE bCommandInfo = 0;                   /* fr DosDevIOCtl(...)        */

   ULONG ulCommandLen = sizeof (BYTE);
   ULONG ulDataLen = sizeof(QUERYGEOMETRY); 
   QUERYGEOMETRY QueryGeometry;             /* Struktur fr DosDevIOCtl    */

   APIRET RetCode;

  
   RetCode = DosDevIOCtl                    /* Geometrie erfragen          */
   (
      hPartHandle,                          /* Partitionshandle            */
      IOCTL_PHYSICALDISK,                   /* Kategorie Disk-Zugriff (9)  */
      DSK_GETDEVICEPARAMS,                  /* Geometrie erfragen          */
      &bCommandInfo,                        /* 0 --> BPB holen             */
      ulCommandLen,                         /* Parameterl„nge              */
      &ulCommandLen,                        /* Parameterl„nge (zurck)     */
      &QueryGeometry,                       /* Datenpaket                  */
      ulDataLen,                            /* Parameterl„nge              */
      &ulDataLen                            /* Parameterl„nge (zurck)     */
   );


   /*---- Geometriedaten bertragen ---------------------------------------*/
   pPartition->ulCylinder = QueryGeometry.usCylinder;
   pPartition->ulHead = QueryGeometry.usHead;
   pPartition->ulSecsTrack = QueryGeometry.usSecsTrack;
 

   return RetCode;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: RBAToGeometry                                                 */
/*                                                                         */
/***************************************************************************/

VOID RBAToGeometry
(
   ULONG ulRBA,                             /* relative Blockadresse       */
   PGEOMETRY pLocation,                     /* Zielkoordinaten             */
   PPARTGEOMETRY pPartition                 /* Partitionsgeometrie         */
)

{
   ULONG ulQuotient;


   pLocation->ulSector = ulRBA % pPartition->ulSecsTrack + 1;
   ulQuotient = ulRBA / pPartition->ulSecsTrack;

   pLocation->ulCylinder = ulQuotient / pPartition->ulHead;
   pLocation->ulHead = ulQuotient % pPartition->ulHead;
}


/***************************************************************************/
/*                                                                         */
/* Funktion: GeometryToRBA                                                 */
/*                                                                         */
/***************************************************************************/

ULONG GeometryToRBA
(
   PGEOMETRY pLocation,                     /* ZKS-Koordinaten             */
   PPARTGEOMETRY pPartition                 /* Partitionsgeometrie         */
)

{
   return 
      pLocation->ulSector - 1 +
      pPartition->ulSecsTrack *
      (pLocation->ulHead + pLocation->ulCylinder * pPartition->ulHead);
}


                                           

