#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "./uthash.h"

struct sectorStruct *sectors = NULL;    /* important! initialize to NULL */

struct sectorStruct {
    int id;                    /* key */
    int sector;
    int track;
    int type;
    unsigned char * sectorData;
    UT_hash_handle hh;         /* makes this structure hashable */
};

void addSectorToHash (int track, int sector, int type, unsigned char * sectorData) {
  struct sectorStruct *s;
  int id;
  s = (struct sectorStruct *) malloc(sizeof *s);
  s->track=track;
  s->sector=sector;
  id=track<<8|sector;
  s->id = id;
  s->type=type;
  s->sectorData = sectorData;
  HASH_ADD_INT(sectors, id, s );
}


struct sectorStruct * findSectorInHash (int track, int sector) {
  int id = track<<8|sector;
  struct sectorStruct *s;
  HASH_FIND_INT(sectors, &id, s);  /* s: output pointer */
  return s;
}
/*
  Create an in memory database of the FLEX disk structure - either read from existing .IMD file or newly created.
  Add options to install bootstrap code
  possibility to add files.
  List all files
  Extract files from a list of files.
  The database would have lookup based on sector / track. Two bytes. Give back a sector.
  Program should be able to write out the database and read it in.
  It should be possible to create new databases based on a format specifier. I.e track 0 FM 10 sector 2 heads. Other tracks MFM 18 sector or whatever.
  This should then be converted into a IMD when written out. The format specifier need to go with the database so 
  that an IMD can be generated.






*/



unsigned char * findStartOfDiskData (unsigned char * imd) {
  while (*imd != 0x1A) {
    imd++;
  }
  imd++;
  return imd;
}

typedef struct trackHeader {
  int modeValue;
  int cylinder;
  int head;
  int haveSectorCylinderMap;
  int haveSectoreHeadMap; 
  int numSectorsInTrack;
  int sectorSize;
} TrackHeader;

TrackHeader readTrackHeader (unsigned char ** disk) {
  TrackHeader t;
  t.modeValue = (int) **disk;
  (*disk)++;
  t.cylinder = (int) **disk;
  (*disk)++;
  t.head = (int) **disk & 0x01;
  t.haveSectorCylinderMap = (int) (**disk & 0x40) >> 6;
  t.haveSectoreHeadMap = (int) (**disk & 0x80) >> 7;
  (*disk)++;
  t.numSectorsInTrack = **disk & 0xff;
  (*disk)++;
  t.sectorSize = 1 << (**disk+7); 
  (*disk)++;
  return t;
}

char * parseModeValue (int modeValue) { 
  switch (modeValue) {
    case 0 : return "500 kbps FM";
    case 1 : return "300 kbps FM";
    case 2 : return "250 kbps FM";
    case 3 : return "500 kbps MFM";
    case 4 : return "300 kbps MFM";
    case 5 : return "250 kbps MFM";
  }
}

void printTrackHeader (TrackHeader t) {
  printf("Mode : %s Cylinder %d Head %d #Sectors %d Sector Size %d\n", parseModeValue(t.modeValue), t.cylinder, t.head, t.numSectorsInTrack, t.sectorSize);  
}

char returnPrintable(unsigned char ch) {
  if (ch>=0x20 && ch <127) {
    return ch;
  } else return 0x20;
}

int main (int argc, char * argv[]) {
  int numSectorsOnTrack0;
  int numSectorsOnTrack1;
  int aflag = 0;
  int bflag = 0;
  char * imdFileName = NULL;
  char * dskFileName = NULL;
  int index;
  int c;
  long sz;
  FILE * imdFile, * dskFile;
  unsigned char * imd, * diskp;
  TrackHeader trackHeader;
  unsigned char sectorNumberingMap [256];
  unsigned char sectorCylinderMap [256];
  unsigned char sectorHeadMap [256];
  int ret; 
  unsigned char * s;
  int compressedData;
  int i;
  struct sectorStruct * t;
  opterr = 0;

  while ((c = getopt (argc, argv, "i:o:")) != -1)
    switch (c)
      {
      case 'i':
        imdFileName = strdup(optarg);
        break;
      case 'o':
        dskFileName = strdup(optarg);
        break;
      case '?':
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        exit(1);
      default:
        abort ();
      }

  printf ("cvalue = %s\n",imdFileName);
  if (imdFileName!=NULL) {
    imdFile = fopen (imdFileName, "r");
    if (imdFile == NULL) {
      fprintf(stderr, "Failed to open file %s\n", imdFileName);
      exit(1);
    }
  }
  if (dskFileName!=NULL) {
    dskFile = fopen(dskFileName, "w");
    if (dskFile == NULL) {
      fprintf(stderr, "Failed to open file %s\n", dskFileName);
      exit(1);
    }
  }

  fseek(imdFile, 0L, SEEK_END);
  sz = ftell(imdFile);  
  fseek(imdFile, 0L, SEEK_SET); 
  imd = (unsigned char *) malloc(sz);
  ret = fread(imd, 1, sz, imdFile);
  if (ret!=sz) {
    fprintf(stderr, "Failed to read in file.");
    exit(1);
  }
  diskp = findStartOfDiskData(imd);
  while (diskp<(imd + sz)) {
    trackHeader = readTrackHeader(&diskp);
    if (trackHeader.cylinder==0 && trackHeader.head == 0) {
      numSectorsOnTrack0 = trackHeader.numSectorsInTrack * 2;
    }
    if (trackHeader.cylinder==1 && trackHeader.head == 0) {
      numSectorsOnTrack1 = trackHeader.numSectorsInTrack * 2;
    }

    printTrackHeader(trackHeader);
    for (int l=0; l<trackHeader.numSectorsInTrack;l++) {
      sectorNumberingMap[l] = *diskp++;  
    }
    if (trackHeader.haveSectorCylinderMap) for (int l=0; l<trackHeader.numSectorsInTrack;l++) {
      sectorCylinderMap[l] = *diskp++;  
    }
    if (trackHeader.haveSectoreHeadMap) for (int l=0; l<trackHeader.numSectorsInTrack;l++) {
      sectorHeadMap[l] = *diskp++;  
    }
    for (int k=0; k<trackHeader.numSectorsInTrack; k++) {
      int type;
      printf("Physical sector# %d Logical sector# %d SectorDataType: %d\n", k, sectorNumberingMap[k], 0xff & *diskp);
      type = 0xff & *diskp++;
      switch (type) {
        case 0:
          s=(unsigned char *) malloc(256);
          memset(s,0, 256);
          addSectorToHash(trackHeader.cylinder, sectorNumberingMap[k], type, s);
        break;
        case 1:
        case 3:
        case 5:
        case 7:
          for (int i=0; i<trackHeader.sectorSize; i+=16) {
            for (int j=0; j<16; j++) {
              printf(" %02X", *(diskp+i+j));
            }
            printf(" ");
            for (int j=0; j<16; j++) {
              printf("%c", returnPrintable(*(diskp+i+j)));
            }
            printf("\n");
          }
          addSectorToHash(trackHeader.cylinder, sectorNumberingMap[k], type, diskp);
          diskp+=trackHeader.sectorSize;
          break;
        case 2:
        case 4:
        case 6:
        case 8: 
          compressedData = *diskp++;
          printf("Compressed data %02X\n",compressedData);
          s=(unsigned char *) malloc(256);
          memset(s,compressedData, 256);
          addSectorToHash(trackHeader.cylinder, sectorNumberingMap[k], type, s);
          break;
      }

    }
  }
  for (i=1;i<=numSectorsOnTrack0;i++) {
    t=findSectorInHash(0,i);
    fwrite(t->sectorData, 256, 1, dskFile);
  }

  for(;i<=numSectorsOnTrack1;i++) {
    int data = 0;
    for (int k=0; k <256; k++) {
      fwrite(&data, 1, 1, dskFile);
    }
  }
  for (int j=1;j<40;j++) {
    for (int i=1;i<=numSectorsOnTrack1;i++) {
      t=findSectorInHash(j,i);
      fwrite(t->sectorData, 256, 1, dskFile);
    }
  }
  fclose(dskFile);
  return 1;
} 
