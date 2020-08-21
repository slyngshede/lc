/*
 * List files in categories (and columns)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/dir.h>

#define	WIDTH	79		/* Default line width */
#define	GAP	1		/* Minimum gap between columns */
#define	INDENT1	4		/* Indent for multiple directories */
#define	INDENT2	4		/* Indent for files in a category */
#define	NFNAME	400		/* Maximum a filename can expand to */

// from source/4.2.x/usr/include/kernel/dir.h
//# define	DIRSIZ		14
#define DIRSIZ MAXNAMLEN

int	oneflag;		/* One per line */
int	aflag;			/* Do all entries, including `.' and `..' */
int	bflag;			/* Do block special */
int	cflag;			/* Do character special */
int	dflag;			/* Do directories */
int	fflag;			/* Do regular files */
int	mflag;			/* Do multiplexor files */
int	pflag;			/* Do pipes */
int	allflag = 1;		/* Do all types */
int	ndir;
int	printed = 0;		/* Set when we have printed something */
int	maxwidth;		/* Maximum width of a filename */
int	lwidth = WIDTH-INDENT2;

struct	stat	sb;
char	obuf[BUFSIZ];
char	fname[NFNAME];

typedef	struct	ENTRY	{
	struct	ENTRY	*e_fp;
	char	e_name[DIRSIZ];
}	ENTRY;

ENTRY	*files, *links, *dirs, *blocks, *chars, *pipes, *mults;

void usage();
void prtype(ENTRY *list, char *type);
void addlist(ENTRY **lpp, ENTRY *ep);
void clearlist(ENTRY **lpp);
int doentry(char *dirname, struct dirent *dp);
void prnames();
int lcdir(char *dname);
int lc(char *name);


/*
 * Print a line with possible indent.
 */
/* VARARGS */
#define prindent(...) \
	if (ndir > 1) \
		printf("%*s", INDENT1, ""); \
	printf(__VA_ARGS__); \
	printed = 1

int main(int argc, char *argv[])
{
	register char *ap;
	register int i;
	register int estat = 0;

	while (argc>1 && *argv[1]=='-') {
		for (ap = &argv[1][1]; *ap!='\0'; ap++)
			switch (*ap) {
			case 'f':
				fflag = 1;
				allflag = 0;
				break;

			case 'd':
				dflag = 1;
				allflag = 0;
				break;

			case 'm':
				mflag = 1;
				allflag = 0;
				break;

			case 'b':
				bflag = 1;
				allflag = 0;
				break;

			case 'c':
				cflag = 1;
				allflag = 0;
				break;

			case 'p':
				pflag = 1;
				allflag = 0;
				break;

			case '1':
				oneflag = 1;
				break;

			case 'a':
				aflag = 1;
				break;

			default:
				usage();
			}
		argv++;
		argc--;
	}
	if (allflag)
		fflag = dflag = cflag = bflag = mflag = pflag = 1;
	setbuf(stdout, obuf);
	if (argc < 2) {
		ndir = 1;
		estat = lc(".");
	} else {
		if (argc > 2)
			lwidth -= INDENT1;
		ndir = argc-1;
		for (i=1; i<argc; i++) {
			estat |= lc(argv[i]);
			fflush(stdout);
		}
	}
	exit(estat);
}

/*
 * Do `lc' on a single name.
 */
int lc(name)
char *name;
{
	char *type;

	if (stat(name, &sb) < 0) {
		fprintf(stderr, "%s: not found\n", name);
		return (1);
	}
	switch (sb.st_mode & S_IFMT) {
	case S_IFDIR:
		return (lcdir(name));

	case S_IFREG:
		type = "file";
		break;

	case S_IFLNK:
		type = "symlink";
		break;

	case S_IFBLK:
		type = "block special file";
		break;

	case S_IFCHR:
		type = "character special file";
		break;

	case S_IFIFO:
		type = "FIFO/pipe";
		break;

	case S_IFSOCK:
		type = "socket";
		break;

	default:
		printf("%s: unknown file type\n", name);
		return (1);
	}
	printf("%s: %s\n", name, type);
	return (0);
}

/*
 * Process one directory.
 */
int lcdir(dname)
char *dname;
{
	register struct dirent *dp;
	register DIR *fd;

	clearlist(&files);
	clearlist(&links);
	clearlist(&dirs);
	clearlist(&blocks);
	clearlist(&chars);
	clearlist(&pipes);
	clearlist(&mults);
	maxwidth = 0;
	if (ndir > 1) {
		if (printed)
			putchar('\n');
		printf("%s:\n", dname);
	}
	printed = 0;
	if (!(fd = opendir(dname))) {
		fprintf(stderr, "Cannot open directory `%s'\n", dname);
		return 1;
	}
	while ((dp = readdir(fd)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		doentry(dname, dp);
	}
	closedir(dir);
	prnames();
	if (nb < 0) {
		fprintf(stderr, "%s: directory read error\n", dname);
		return (1);
	}
	return (0);
}

/*
 * Do a stat on one filename in the
 * indicated directory.
 * and then sort into the appropriate table.
 */
int doentry(dirname, dp)
char *dirname;
struct dirent *dp;
{
	int width = 0;

	{
		register char *p = dp->d_name;

		if (!aflag
		 && *p++=='.' && (*p=='\0' || (*p++=='.' && *p=='\0')))
			return (0);
	} {
		register char *p1, *p2;
		register unsigned n = DIRSIZ;

		p1 = fname;
		p2 = dirname;
		while (*p1++ = *p2++)
			;
		p1--;
		if (*p1 != '/')
			*p1++ = '/';
		p2 = dp->d_name;
		do {
			if (*p2 == '\0')
				break;
			width++;
			*p1++ = *p2++;
		} while (--n);
		*p1 = '\0';
	}
	if (width > maxwidth)
		maxwidth = width;
	if (stat(fname, &sb) < 0) {
		prindent("%.*s: cannot stat\n", DIRSIZ, dp->d_name);
		return (1);
	} {
		register ENTRY *ep;
		register ENTRY **list;

		if ((ep = (ENTRY *)malloc(sizeof (ENTRY))) == NULL) {
			fprintf(stderr, "Out of memory\n");
			exit (1);
		}
		strncpy(ep->e_name, dp->d_name, DIRSIZ);
		switch (sb.st_mode & S_IFMT) {
		case S_IFREG:
			list = &files;
			break;

		case S_IFLNK:
			list = &links;
			break;

		case S_IFDIR:
			list = &dirs;
			break;

		case S_IFBLK:
			list = &blocks;
			break;

		case S_IFCHR:
			list = &chars;
			break;

		case S_IFSOCK:
			list = &mults;
			break;

		case S_IFIFO:
			list = &pipes;
			break;

		default:
			prindent("%.*s: unknown file type\n", DIRSIZ,
			    dp->d_name);
			return (1);
		}
		addlist(list, ep);
	}
	return (0);
}

/*
 * Add an entry to the list.
 * Sort the list at insertion time
 * into lexicographic order.
 */
void addlist(lpp, ep)
ENTRY **lpp;
ENTRY *ep;
{
	register ENTRY *rp;
	register ENTRY *pp;

	for (pp=NULL, rp=*lpp; rp!=NULL; pp=rp, rp=rp->e_fp)
		if (strncmp(ep->e_name, rp->e_name, DIRSIZ) <= 0)
			break;
	if (pp == NULL) {
		ep->e_fp = *lpp;
		*lpp = ep;
	} else {
		ep->e_fp = pp->e_fp;
		pp->e_fp = ep;
	}
}

/*
 * Clear out the list, a pointer to which is
 * the argument.
 */
void clearlist(lpp)
ENTRY **lpp;
{
	register ENTRY *rp, *op;

	rp = *lpp;
	while (rp != NULL) {
		op = rp;
		rp = rp->e_fp;
		free(op);
	}
	*lpp = NULL;
}

/*
 * Print out the appropriate names.
 */
void prnames()
{
	if (dflag)
		prtype(dirs, "Directories");
	if (fflag) {
		prtype(files, "Files");
		prtype(links, "Symlinks");
	}
	if (cflag)
		prtype(chars, "Character special files");
	if (bflag)
		prtype(blocks, "Block special files");
	if (pflag)
		prtype(pipes, "Pipes");
	if (mflag)
		prtype(mults, "Multiplexed files");
}

/*
 * Print out one type of files
 */
void prtype(list, type)
ENTRY *list;
char *type;
{
	register char *cp;
	register ENTRY *ep;
	register int n;
	register int i;
	int npl;

	if (list == NULL)
		return;
	if (printed)
		putchar('\n');
	if(!oneflag)
		prindent("%s:\n", type);
	if (oneflag)
		npl = 1;
	else
		npl = lwidth/(maxwidth+GAP);
	ep = list;
	i = 0;
	for (ep = list; ep != NULL; ep = ep->e_fp) {
		if (!oneflag && (i == 0)) {
			printf("%*s", INDENT2, "");
			prindent("");
		}
		n = DIRSIZ;
		for (cp=ep->e_name; *cp!='\0' && n; n--)
			putchar(*cp++);
		if (++i!=npl && ep->e_fp!=NULL) {
			n -= DIRSIZ-GAP-maxwidth;
			while (n-- > 0)
				putchar(' ');
		} else {
			i = 0;
			putchar('\n');
		}
	}
}

void usage()
{
	fprintf(stderr, "Usage: lc [-afdcbp] [-1] [name ...]\n");
	exit(1);
}
