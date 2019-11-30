#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

enum {
	READ_END = 0,
	WRITE_END = 1,
};

typedef char *String;

typedef struct {
	String stdout, stderr;
	int status;
} ProcessOutput;

typedef struct {
	pid_t pid;
	int stdin[2], stdout[2], stderr[2];
	ProcessOutput output;
} Process;


static void SwapIoFd(int io[2], int pipe_end, int oldfd) {
	/* Swaps the fd in oldfd with io[pipe_end] and closes the opposite pipe_end */
	int newfd = io[pipe_end];
	if (dup2(newfd, oldfd) == -1) abort();

	int closefd = (pipe_end == READ_END) ? io[WRITE_END] : io[READ_END];
	close(closefd);
}

static void MakeChild(Process process) {
	/* Sets a forked process up as a child */
	SwapIoFd(process.stdin, READ_END, STDIN_FILENO);
	SwapIoFd(process.stdout, WRITE_END, STDOUT_FILENO);
	SwapIoFd(process.stderr, WRITE_END, STDERR_FILENO);

    char *grep = getenv("SPLOSH_GREP_PATH");

	execl(grep, "grep", "hihelo", NULL);
}

static Process MakeParent(Process *process) {
	/* Sets a forked process up as a parent */
	close(process->stdin[READ_END]);
	close(process->stdout[WRITE_END]);
	close(process->stderr[WRITE_END]);

	return *process;
}

static void CreatePipes(Process *process) {
	/* Iterates over the pipe fd's in process and ensures they can be set up */
	int *ios[] = {
		process->stdin,
		process->stdout,
		process->stderr,
		NULL
	};

	int **io;
	for (io = ios; *io != NULL; io++) {
		if(pipe(*io) == -1) abort();
	}
}

Process NewProcess() {
	/* Spawn a new Process */
	Process process;

	CreatePipes(&process);

	process.pid = fork();

	if (process.pid == -1) abort();

	if (process.pid == 0) MakeChild(process);

	return MakeParent(&process);
}

ssize_t WriteToProcess(Process *process, const char *msg) {
	/* Write data to a process */
	return write(process->stdin[WRITE_END], msg, strlen(msg));
}

String NewString(const char *format, ...) {
	va_list size_args, format_args;
	va_start(format_args, format);
	va_copy(size_args, format_args);

	int size = 1 + vsnprintf(NULL, 0, format, size_args);
	va_end(size_args);

	String str = calloc(size, sizeof(char));
	vsnprintf (str, size, format, format_args);
	va_end(format_args);

	return str;
}

String GetIoOutput(int fd) {
	char buf[1024];
	ssize_t bytes;
	String EMPTY_STRING = "", str = EMPTY_STRING;
	while (bytes = read(fd, buf, sizeof(buf))) {
		String tmp = NewString("%s%.*s", str, (int) bytes, buf, buf);
		
        if (str != EMPTY_STRING) free(str);
		str = tmp;

	}

	return (str == EMPTY_STRING) ? NULL : str;
}

int CloseProcess(Process *process) {
	/* Close the Process and collate the data */
	close(process->stdin[WRITE_END]);

	process->output.stdout = GetIoOutput(process->stdout[READ_END]);
	process->output.stderr = GetIoOutput(process->stderr[READ_END]);
	waitpid(process->pid, &process->output.status, 0);

	return process->output.status;
}

const int SPLOSH_MAJOR_VERSION	= 0;
const int SPLOSH_MINOR_VERSION = 0;
const char *SPLOSH_CODENAME = "pre-alpha";

void ParseArgs(int argc, char *argv[])	{
	/* Parses the Args, not sure if there even needs to be any */
	int opt;
	while((opt = getopt(argc, argv, "@")) != -1) {
		switch (opt) {
			case '@':
				fprintf(stderr, "splosh:%d.%d:%s\n",
					SPLOSH_MAJOR_VERSION,
					SPLOSH_MINOR_VERSION,
					SPLOSH_CODENAME
				);
				exit(0);
				break;
		}
	}

}

int main(int argc, char *argv[]) {
	Process process = NewProcess();

	ParseArgs(argc, argv);

    String msg = GetIoOutput(STDIN_FILENO);

	WriteToProcess(&process, msg);
	CloseProcess(&process);

	const char *GREEN_TEXT = "\x1B[32m" "%s" "\x1B[0m";
	const char *RED_TEXT = "\x1B[31m" "%s" "\x1B[0m";

	fprintf(stdout, GREEN_TEXT, process.output.stdout);
	fprintf(stderr, RED_TEXT, process.output.stderr);

	return process.output.status;
}
