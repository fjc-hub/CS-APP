/* 
 * tsh - A tiny shell program with job control
 * 
 * Jiacheng FU
 * 1661007260@qq.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

volatile sig_atomic_t fg_pid_exit_flag; /* if > 0, indicate the current FG process has been reaped by singal handler. else not */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* Sio (Signal-safe I/O) routines */
ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
void sio_error(char s[]);

/* Sio wrappers */
ssize_t Sio_puts(char s[]);
ssize_t Sio_putl(long v);
void Sio_error(char s[]);
ssize_t Sio_write_event(int jid, pid_t pid, char* event, int sig);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	        break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	        break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	        break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            unix_error("fgets cmdline");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        // ignore empty command
        if (cmdline == NULL || cmdline[0] == '\n')
            continue;

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    // calculate the mask of signals to block
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD|SIGINT|SIGTSTP);

    char **argv = malloc(sizeof(*argv)*MAXARGS);
    int bg = parseline(cmdline, argv);
    if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "jobs") || 
        !strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        // built-in commands
        builtin_cmd(argv);
    } else {
        // executable commands
        // check if the executable file exists
        FILE *fp = fopen(argv[0], "r");
        if (fp == NULL) {
            printf("%s: Command not found\n", argv[0]);
            return;
        } else {
            fclose(fp);
        }

        pid_t pid;
        sigset_t mask_reap;
        sigemptyset(&mask_reap);
        sigaddset(&mask_reap, SIGCHLD);
        // block SIGCHLD signal(reap child in handler), be sure to addjob() must be called before deletejob()
        sigprocmask(SIG_BLOCK, &mask_reap, &prev_mask);
        
        if ((pid = fork()) == 0) {
            // child process
            // 1.unblock SIGCHLD signal of child, because it inherits signal mask from parent
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            // 2.move child to a new process group, to avoid parent(shell) receiving SIGINT(ctrl+c)
            setpgid(0, 0);
            // 3.load and run program
            if (execve(argv[0], argv, environ) < 0) { // exec*() will set signal handler back to default handler, but signal block mask not
                unix_error("eval execve");
            }
            // the child process certainly not reach here
        } else if (pid < 0) {
            unix_error("eval fork error");
            return ;
        }

        if (!bg) {
            sigprocmask(SIG_BLOCK, &mask, NULL); // block signals with races(global variable jobs) to avoid interrupting
            addjob(jobs, pid, FG, cmdline); // add job as FG

            // parent waits for foreground job to terminate
            waitfg(pid);

            sigprocmask(SIG_SETMASK, &prev_mask, NULL); // restore the signals

        } else {
            sigprocmask(SIG_BLOCK, &mask, NULL); // block signals with races(global variable jobs) to avoid interrupting
            addjob(jobs, pid, BG, cmdline); // add job as BG
            sigprocmask(SIG_SETMASK, &prev_mask, NULL); // restore the signals

            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
    }
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    }
    else {
	    delim = strchr(buf, ' ');
    }

    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if (!strcmp(argv[0], "quit")) {
        exit(0);
    } else if (!strcmp(argv[0], "jobs")) {
        listjobs(jobs);
    } else if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        do_bgfg(argv);
    } else {
        app_error("invalid built-in command"); /* not a builtin command */
    }
    return 0;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    pid_t pid;
    if (!strncmp(argv[1], "%", 1)) {
        struct job_t* job = getjobjid(jobs, atoi(argv[1]+1));
        if (job == NULL) {
            printf("%s: No such job\n", argv[1]);
            return;
        }
        pid = job->pid;
    } else {
        pid = atoi(argv[1]);
    }
    if (!strcmp(argv[0], "fg")) {
        sigset_t mask, prev_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &prev_mask); // block SIGCHLD signal
        
        // set job's state to FG
        struct job_t* job = getjobpid(jobs, pid);
        job->state = FG;

        // send SIGCONT
        if (kill(-pid, SIGCONT) < 0) {
            unix_error("kill SIGCONT");
        }
        // waiting for FG
        waitfg(pid);
        
        sigprocmask(SIG_SETMASK, &prev_mask, NULL); // restore signals
    } else {
        // send SIGCONT
        if (kill(-pid, SIGCONT) < 0) {
            unix_error("kill SIGCONT");
        }
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    // unblock SIGINT and SIGTSTP before waitfg after addjob, so that sigsuspend() can be interrupted
    sigset_t unlock_mask, prev_mask;
    sigemptyset(&unlock_mask);
    sigaddset(&unlock_mask, SIGINT|SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &unlock_mask, &prev_mask);

    // block SIGCHLD signal to protect update global variable fg_pid_exit_flag
    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    // unblock SIGCHLD signal, in order to suspend() can return after when SIGCHLD signal handler returns
    sigset_t mask;
    sigemptyset(&mask);
    while(!fg_pid_exit_flag) {
        sigsuspend(&mask); // atomic operation. suspend current thread, as for time of return, refering to man 7 sigsuspend 
    }

    fg_pid_exit_flag = 0; // reset flag, must be reset in SIGCHLD-uninterrupted code block???

    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // restore signals

    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int old_errno = errno; // save errno
    errno = 0; // reset
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD|SIGINT|SIGTSTP);

    // obtain status information of children processes
    // no block calling handler | obtain getting stopped children | obtain getting continued children
    int status;
    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        /* block signals to avoid interrupting by other signal_handler 
           that has race condition(e.g. access global jobs) with this handler */
        sigprocmask(SIG_BLOCK, &mask, &prev_mask);

        struct job_t *job = getjobpid(jobs, pid);
        if (WIFSTOPPED(status)) { // deal with stopped child
            if (job->state == FG) {
                fg_pid_exit_flag = pid; // inform main shell not to wait FG process
            }
            job->state = ST;
            Sio_write_event(job->jid, job->pid, "stopped", 20);
        } else if (WIFCONTINUED(status)) { // deal with continued child 
            if (job->pid == fgpid(jobs)) {
                job->state = FG;
            } else {
                job->state = BG;
            }
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) { // deal with terminated child    
            if (job->state == FG) {
                fg_pid_exit_flag = pid; // set the global fg-reaped flag to notify main process to wake up
            }
            if (WIFSIGNALED(status)) 
                Sio_write_event(job->jid, job->pid, "terminated", 2);
            deletejob(jobs, pid); // delete must be after if-statement, because deletejob will clear job structure pointed to by *job
        } else {
            Sio_puts("sigchld_handler unexpected status\n");
        }

        // restore signals
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    }
    
    // deal error if exists
    if (errno != 0 && errno != ECHILD) { // ECHILD means no child process
        Sio_puts(" sigchld_handler error: ");
        Sio_putl((long)errno);
        Sio_puts(strerror(errno));
        Sio_puts("\n");
    }

    errno = old_errno; // restore errno

    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    int old_errno = errno; // save errno
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD|SIGINT|SIGTSTP);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask); // block signals

    pid_t pid = fgpid(jobs);
    if (pid > 0) {
        if (kill(-pid, SIGINT) < 0) {
            Sio_puts("sigint_handler: kill failed\n");
        }
    }
    

    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // restore signals
    errno = old_errno; // restore errno
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    int old_errno = errno; // save errno
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD|SIGINT|SIGTSTP);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask); // block signals

    pid_t pid = fgpid(jobs);
    if (pid > 0) {
        if (kill(-pid, SIGTSTP) < 0) {
            Sio_puts("sigtstp_handler: kill failed\n");
        }
    }

    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // restore signals
    errno = old_errno; // restore errno
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
	v = -v;

    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
	s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s));
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */ 
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);
}

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v)
{
    ssize_t n;
  

    if ((n = sio_putl(v)) < 0)
	sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
  
    if ((n = sio_puts(s)) < 0)
	sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}

// safe to write output in asynchronized signal handler
ssize_t Sio_write_event(int jid, pid_t pid, char* event, int sig) {
    ssize_t n = 0;
    n += Sio_puts("[");
    n += Sio_putl((long)jid);
    n += Sio_puts("] ");
    n += Sio_puts("(");
    n += Sio_putl((long)pid);
    n += Sio_puts(") ");
    n += Sio_puts(event);
    n += Sio_puts(" by signal ");
    n += Sio_putl((long)sig);
    n += Sio_puts("\n");
    return n;
}