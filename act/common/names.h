/*************************************************************************
 *
 *  Disk hash table for names and aliases
 *
 *  Copyright (c) 2008, 2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#ifndef __NAMES_H__
#define __NAMES_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 
   Names file database. Keeps the names on disk in tables, caching
   names as and when necessary.

   Disk format:
     <file>_str.dat  <- string table
                        * contains null-terminated strings
                        * a null string indicates `end of record'

     <file>_idx.dat  <- index table
*/

#define IDX_TYPE unsigned int
#define IDX_SIZE 4

#define NAMES_WRITE 1
#define NAMES_READ  2

typedef struct {
  char *string_tab;		/* string table file name */
  FILE *sfp;			/* file pointer */

  char *idx_tab;		/* index table file name */
  FILE *ifp;			/* file pointer */
  
  char *idx_revtab;		/* reverse index table */
  FILE *rfp;

  char *alias_tab;		/* alias table */
  FILE *afp;

  IDX_TYPE fpos;		/* position in string table */

  unsigned int mode;		/* read or write */

  IDX_TYPE count;
  IDX_TYPE hsize;		/* size of the disk inverse hash
				   table! */

  int reverse_endian;		/* 1 if endianness should be reversed
				   on reading from disk */

  IDX_TYPE unique_names;	/* actual # of strings */

  unsigned int update_hash;	/* 1 if hash has been updated! */

} NAMES_T;

/*
  Return # that corresponds to the string given
    -1 = not found
*/
IDX_TYPE names_str2name (NAMES_T *N, char *s);

IDX_TYPE names_parent (NAMES_T *N, IDX_TYPE num);

/*
  Return string corresponding to number, NULL if not found
  NOTE: this string will be DESTROYED if some other names_ function is
  called!
*/
char *names_num2name (NAMES_T *N, IDX_TYPE num);


NAMES_T *names_open (char *file);

/*
  1. Create names file.
  2. (names_newname; names_addalias*)*
  3. names_close ()
*/
NAMES_T *names_create (char *file, IDX_TYPE max_names);
IDX_TYPE names_newname (NAMES_T *, char *str);
void names_addalias (NAMES_T *, IDX_TYPE idx1, IDX_TYPE idx2);
void names_close (NAMES_T *);

#ifdef __cplusplus
}
#endif

#endif /* __NAMES_H__ */
