#include "lib.h"
#include <args.h>

int debug_ = 0;

#define debug 0

// get the next token from string s
// set *p1 to the beginning of the token and
// *p2 just past the token.
// return:
//	0 for end-of-string
//	> for >
//	| for |
//	w for a word
//
// eventually (once we parse the space where the nul will go),
// words get nul-terminated.
#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

int
_gettoken(char *s, char **p1, char **p2)
{
	int t;

	if (s == 0) return 0;

	*p1 = 0;
	*p2 = 0;

	while (strchr(WHITESPACE, *s)) *s++ = 0;

	if (*s == 0) {
		return 0;
	}
	if (*s == '"') {
		s++;
		*p1 = s;
		while (*s && (*s != '"')) s++;
		*s = 0;
		while (*s && !strchr(WHITESPACE SYMBOLS, *s)) s++;
		*p2 = s;
		return 'w';
	}
	if (strchr(SYMBOLS, *s)){
		t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}
	
	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) s++;
	*p2 = s;
	if (debug_ > 1) {
		t = **p2;
		**p2 = 0;
		**p2 = t;
	}
	return 'w';
}

int
gettoken(char *s, char **p1)
{
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 16
/*** exercise 6.6 ***/
void
runcmd(char *s)
{
	char *argv[MAXARGS], *t;
	int argc, c, i, r, p[2], fdnum, rightpipe;
	int envid;
	struct Stat state;
	int is_parallel = 0;

	rightpipe = 0;
	gettoken(s, 0);
again:
	argc = 0;
	for(;;){
		c = gettoken(0, &t);
		switch(c){
		case 0:
			goto runit;
		case 'w':
			if(argc == MAXARGS){
				writef("too many arguments\n");
				exit();
			}
			argv[argc++] = t;
			break;
		case '<':
			if(gettoken(0, &t) != 'w') {
				writef("syntax error: < not followed by word\n");
				exit();
			}
			// Your code here -- open t for reading,
			// dup it onto fd 0, and then close the fd you got.
			/*if ((r = stat(t, &state))) {
				writef("failed to get file state, file may not exist\n");
				exit();
			}
			if (state.st_isdir) {
				writef("source should be file, not directory\n");
				exit();
			}*/
			if ((fdnum = open(t, O_RDONLY)) < 0) {
				writef("failed to open file\n");
				exit();
			}
			dup(fdnum, 0);
			close(fdnum);
			break;
		case '>':
			if(gettoken(0, &t) != 'w') {
				writef("syntax error: > not followed by word\n");
				exit();
			}
			// Your code here -- open t for writing,
			// dup it onto fd 1, and then close the fd you got.
			r = stat(t, &state); // file can be unfounded, then we create one
			if (r >= 0 && state.st_isdir) {
				writef("target should be file, not directory\n");
				exit();
			}
			if ((fdnum = open(t, O_WRONLY | O_CREAT)) < 0) {
				writef("failed to open file\n");
				exit();
			}
			dup(fdnum, 1);
			close(fdnum);
			break;
		case '|':
			// Your code here.
			// 	First, allocate a pipe.
			//	Then fork.
			//	the child runs the right side of the pipe:
			//		dup the read end of the pipe onto 0
			//		close the read end of the pipe
			//		close the write end of the pipe
			//		goto again, to parse the rest of the command line
			//	the parent runs the left side of the pipe:
			//		dup the write end of the pipe onto 1
			//		close the write end of the pipe
			//		close the read end of the pipe
			//		set "rightpipe" to the child envid
			//		goto runit, to execute this piece of the pipeline
			//			and then wait for the right side to finish
			pipe(p);
			rightpipe = fork();
			if (rightpipe == 0) { // child_env: right side of pipe
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				goto again;
			} else {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				goto runit;
			}
			// user_panic("| not implemented");
			break;
		case '&':
			is_parallel = 1;
			break;
		case ';':
			envid = fork();
			if (envid == 0) { // child_env
				is_parallel = 0;
				goto runit;
			} else {	// parent_env
				wait(envid);
				argc = 0;
				rightpipe = 0;
				is_parallel = 0;
				do {
					close(0);
					if ((fdnum = opencons()) < 0)
						user_panic("error in opencons\n");
				} while (fdnum); // continue if fd != 0
				dup(0, 1);
			}
			break;
		}
	}

runit:
	if(argc == 0) {
		if (debug_) writef("EMPTY COMMAND\n");
		return;
	}
	argv[argc] = 0;
	if (1) {
		if (debug) writef("[%08x] SPAWN:", env->env_id);
		for (i=0; argv[i]; i++)
			writef(" %s", argv[i]);
		writef("\n");
	}

	if ((r = spawn(argv[0], argv)) < 0)
		writef("\033[031mshell: command \"%s\" not found.\033[0m failed to spawn: %e\n", argv[0], r);

	close_all(); // close all fd

	if (r >= 0) {
		if (debug_) writef("[%08x] WAIT %s %08x\n", env->env_id, argv[0], r);
		
		if (is_parallel == 0) wait(r);
		else {
			writef("\033[33m[%08x]\033[0m\t", r);
			for (i = 0; i < argc; ++i) writef("%s ", argv[i]);
			writef("\n");
			envid = fork();
			if (envid == 0) {
				wait(r);
				writef("\033[33m[%08x]\033[35m\tDone\x1b[0m\n", r);
				exit();
			}
		}
	}
	if (rightpipe) {
		if (debug_) writef("[%08x] WAIT right-pipe %08x\n", env->env_id, rightpipe);
		wait(rightpipe);
	}

	exit();
}

char buf[1024];
//char pwd[1024];
int history_index;
int history_select;

#define USER_TITLE "excalibur-xrb@shell"
#define MAX_HISTORY_NUM 1024

void save_cmd(char *buf) {
	if (buf[0] == 0) return;

	int fd = open(".history", O_WRONLY | O_CREAT | O_APPEND);
	if (fd < 0) {
		fwritef(1, "Failed to open \".history\"\n");
		return;
	}
	fwritef(fd, "%4d  %s\n", history_index, buf);
	history_index = (history_index + 1) % MAX_HISTORY_NUM;
	close(fd);
}

int get_cmd(char *buf, int selnum) {
	if (selnum >= history_index) return 0;

	struct Stat state;
	int fd;
	char history_buf[MAX_HISTORY_NUM * 1024];
	int i, j, cnt, start;

	if ((fd = open(".history", O_RDONLY)) < 0) {
		fwritef(1, "Failed to open \".history\"\n");
		return 0;
	}
	if ((stat(".history", &state)) < 0) {
		fwritef(1, "Failed to get state of \".history\"\n");
		return 0;
	}
	read(fd, history_buf, state.st_size); // let us assume it won't overflow
	history_buf[state.st_size] = 0;
	close(fd);

	for (i = 0, cnt = 0; history_buf[i]; ++i) {
		if (history_buf[i] == '\n') {
			cnt++;
			continue;
		}
			
		if (cnt == selnum) {
			start = i + 6;
			for (j = 0; history_buf[start+j] != '\n'; ++j)
				buf[j] = history_buf[start+j];
			buf[j] = 0;
			return j;
		}
	}
}

void
readline(char *buf, u_int n)
{
	int i, r;

	r = 0;
	for (i = 0; i < n; i++) {
		if ((r = read(0, buf + i, 1)) != 1) {
			if(r < 0) writef("read error: %e", r);
			exit();
		}
		if (buf[i] == 127) { // why not '\b'?
			if (i > 0) {
				fwritef(1, "\033[1D"); // operate cursor
				fwritef(1, "\033[K");
				i -= 2;
			} else i = -1;
		}
		if (buf[i] == '\t') {
			buf[i--] = 0;
			writef("[%s]", buf);
		}
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			return;
		}

		if (buf[i] == 27) {	// keyboard "up arrow"
			while (strlen(buf) != 0) {
				fwritef(1, "\033[1D");
				buf[strlen(buf) - 1] = 0;
			}
			fwritef(1, "\033[1D");
			fwritef(1, "\033[K");

			history_select--;
			if (history_select < 0) 
				history_select = 0;
			i = get_cmd(buf, history_select) - 1;
			fwritef(1, " %s", buf);
		}
		if (buf[i] == '`') { // keyboard "down arrow"
			while (strlen(buf) != 0) {
				fwritef(1, "\033[1D");
				buf[strlen(buf) - 1] = 0;
			}
			fwritef(1, "\033[1D");
			fwritef(1, "\033[K");

			history_select++;
			if (history_select > history_index)
				history_select = history_index;
			i = get_cmd(buf, history_select) - 1;
			fwritef(1, " %s", buf);
		}
	}
	writef("line too long\n");
	while((r = read(0, buf, 1)) == 1 && buf[0] != '\n')
		;
	buf[0] = 0;
}	

void
usage(void)
{
	writef("usage: sh [-dix] [command-file]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int r, interactive, echocmds;
	interactive = '?';
	echocmds = 0;
	pwd[0] = '/';
	history_index = 0;

	writef("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	writef("::                                                         ::\n");
	writef("::              Super Shell  V0.0.0_1                      ::\n");
	writef("::                                                         ::\n");
	writef(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	
	ARGBEGIN{
	case 'd':
		debug_++;
		break;
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}ARGEND

	if (argc > 1) usage();
	if (argc == 1) {
		close(0);
		if ((r = open(argv[1], O_RDONLY)) < 0)
			user_panic("open %s: %e", r);
		user_assert(r==0);
	}
	if (interactive == '?') interactive = iscons(0);

	for(;;){
		if (interactive) 
			fwritef(1, "\n\033[32;1m%s\033[0m:\033[34;1m%s\033[0m$ ", USER_TITLE, pwd);
		
		history_select = history_index;

		//writef("\n[%s]", buf);
		readline(buf, sizeof buf);
		//writef("\n[%s]", buf);

		if (buf[0] == '#' | buf[0] == 0) continue;

		save_cmd(buf);

		if (echocmds) fwritef(1, "# %s\n", buf);

		if (buf[0] == 'c' && buf[1] == 'd') {
			strcat(pwd, buf + 3);
			strcat(pwd, "/");
			continue;
		}

		if ((r = fork()) < 0) user_panic("fork: %e", r);

		if (r == 0) {
			runcmd(buf);
			exit();
			return;
		} else wait(r);
	}
}

