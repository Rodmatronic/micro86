#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>

/* whoami -- print effective userid
   Copyright (C) 89, 90, 91, 92, 93, 1994 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Equivalent to `id -un'. */
/* Written by Richard Mlynarik. */

static void
usage ()
{
  printf ("usage: whoami\n");
  exit (1);
}

int
main (int argc, char *argv[])
{
  register struct passwd *pw;
  register uid_t uid;

  if (argc >= 2)
      usage();

  uid = geteuid();
  pw = getpwuid(uid);
  if (pw) {
      printf("%s\n", pw->pw_name);
      exit(0);
    }
  fprintf (stderr, "whoami: cannot find username for UID %u\n", (unsigned) uid);
  exit (1);
}
