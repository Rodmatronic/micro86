#include "../include/user.h"
#include "../include/errno.h"
/* The name this program was run with. */
char *program_name;

static void
usage (status)
     int status;
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n",
	     program_name);
  else
    {
      printf ("Usage: %s [OPTION]... [STRING]...\n", program_name);
      printf ("\
\n\
  --help      display this help and exit\n\
  --version   output version information and exit\n\
\n\
Without any STRING, assume `y'.\n\
");
    }
  exit (status);
}

void
main (argc, argv)
     int argc;
     char **argv;
{
  program_name = argv[0];

  parse_long_options (argc, argv, "yes", version_string, usage);

  if (argc == 1)
    while (1)
      puts ("y");

  while (1)
    {
      int i;

      for (i = 1; i < argc; i++)
	{
	  puts (argv[i], stdout);
	  putchar (i == argc - 1 ? '\n' : ' ');
	}
    }
}
