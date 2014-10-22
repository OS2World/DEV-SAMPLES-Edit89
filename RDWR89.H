/***************************************************************************/
/*                                                                         */
/* Modul: RdWr89.H                                                         */
/* Autor: Michael Schmidt                                                  */
/* Datum: 11.12.1994                                                       */
/*                                                                         */
/***************************************************************************/

/*---- Datentypen ---------------------------------------------------------*/
typedef struct _GEOMETRY                    /* Struktur fr Zyl., Kopf, Sec*/
{
   ULONG ulCylinder;                        /* Zylinder                    */
   ULONG ulHead;                            /* Kopf                        */
   ULONG ulSector;                          /* Sektor                      */
}
   GEOMETRY, *PGEOMETRY;

typedef struct _PARTGEOMETRY                /* Partitionsgeometrie         */
{
   ULONG ulCylinder;                        /* Zylinder                    */
   ULONG ulHead;                            /* Kopf                        */
   ULONG ulSecsTrack;                       /* Sektoren pro Spur           */
}
   PARTGEOMETRY, *PPARTGEOMETRY;

/*---- Funktionsprototypen ------------------------------------------------*/
APIRET LOG1_ReadSectors(CHAR chDrive, ULONG ulSecPos, ULONG ulSecCount,
   PVOID pvReadBuffer, BOOL boLock);        /* logisches Lesen             */
APIRET LOG1_WriteSectors(CHAR chDrive, ULONG ulSecPos, ULONG ulSecCount,
   PVOID pvWriteBuffer, BOOL boLock);       /* logisches Schreiben         */
APIRET LOG1_RBAToGeometry(CHAR chDrive, ULONG ulRBA, PGEOMETRY pGeometry, 
   PULONG pulHiddenSecs);                   /* Umrechnung Sektor --> ZKS   */
APIRET LOG1_GeometryToRBA(CHAR chDrive, PGEOMETRY pGeometry, PULONG pulRBA, 
   PULONG pulHiddenSecs);                   /* Umrechnung ZKS --> Sektor   */

APIRET LOG2_ReadBytes(CHAR chDrive, ULONG ulSecPos, ULONG ulSize,
   PVOID pvReadBuffer);                     /* logisches Lesen             */
APIRET LOG2_WriteBytes(CHAR chDrive, ULONG ulSecPos, ULONG ulSize,
   PVOID pvWriteBuffer);                    /* logisches Schreiben         */
APIRET LOG2_RBAToGeometry(CHAR chDrive, ULONG ulRBA, PGEOMETRY pGeometry,
   PULONG pulHiddenSecs);                   /* Umrechnung RBA --> ZKS      */
APIRET LOG2_GeometryToRBA(CHAR chDrive, PGEOMETRY pGeometry, PULONG pulRBA,
   PULONG pulHiddenSecs);                   /* Umrechnung ZKS --> RBA      */

APIRET ABS_ReadSectors(CHAR chDrive, ULONG ulSecPos, ULONG ulSecCount,
   PVOID pvReadBuffer, BOOL boLock);        /* physikalisches Lesen        */
APIRET ABS_WriteSectors(CHAR chDrive, ULONG ulSecPos, ULONG ulSecCount,
   PVOID pvWriteBuffer, BOOL boLock);       /* physikalisches Schreiben    */
APIRET ABS_RBAToGeometry(CHAR chDrive, ULONG ulRBA, PGEOMETRY pGeometry);
                                            /* Umrechnung RBA --> ZKS      */
APIRET ABS_GeometryToRBA(CHAR chDrive, PGEOMETRY pGeometry, PULONG pulRBA);
                                            /* Umrechnung ZKS --> RBA      */

/*---- low-level Formatumrechnungen RBA <--> ZKS --------------------------*/
VOID RBAToGeometry(ULONG ulRBA, PGEOMETRY pLocation, 
   PPARTGEOMETRY pPartition);
ULONG GeometryToRBA(PGEOMETRY pLocation, PPARTGEOMETRY pPartition);



/*---- allgemeine Konstanten ----------------------------------------------*/
#define SECTOR_SIZE          0x200          /* 512 Byte pro Sektor         */


