/* Created by SHAOS on 2018-01-21 based on glue.c from Piotr Piatek */

#include "make_mk85_rom.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

bool make_mk85_rom(const char* fileName, unsigned fileSize, const char* buf, size_t body_size, char* error_buf, size_t error_buf_size)
{
    FILE *f;
    unsigned char bajt1, bajt2;
    signed short checksum;
    unsigned int i;

    f = fopen(fileName,"wb");
    if(f == NULL)
    {
        snprintf(error_buf, error_buf_size, "cannot create output file '%s'\n", fileName);
        return false;
    }

    checksum = 0;

    for (i=0; i<fileSize-2; i+=2)
    {
      if(i < body_size)
      {
        bajt1 = (unsigned char) buf[i];
        bajt2 = (unsigned char) buf[i+1];
      }
      else bajt1 = bajt2 = 0;

      if (i<0x100 || i>0x107)
      {
        checksum += bajt1;
        checksum += bajt2 << 8;
      }

      fputc(bajt1,f);
      fputc(bajt2,f);
    }

    checksum = -checksum;
    printf("MK85 checksum: 0x%4.4X (%i)\n",checksum&0xFFFF,checksum);
    fputc(checksum & 0xFF, f);
    fputc((checksum>>8) & 0xFF, f);

    fclose(f);
    return true;
}
