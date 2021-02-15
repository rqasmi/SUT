#ifndef __SUT_H__
#define __SUT_H__
#include <stdbool.h>
#include <ucontext.h>

#define MAX_THREADS 32
#define THREAD_STACK_SIZE 1024 * 64
#define BUFFER_SIZE 1024

typedef struct __threaddesc
{
	int threadid;
	char *threadstack;
	void *threadfunc;
	ucontext_t threadcontext;
} threaddesc;

typedef struct open_message {
  char buf[BUFFER_SIZE];
  int port;
} open_message;

typedef struct write_message {
  char buf[BUFFER_SIZE];
  int size;
} write_message;

extern int numthreads;
extern threaddesc *threadarr[MAX_THREADS];

typedef void (*sut_task_f)();

void sut_init();
bool sut_create(sut_task_f fn);
void sut_yield();
void sut_exit();
void sut_open(char *dest, int port);
void sut_write(char *buf, int size);
void sut_close();
char *sut_read();
void sut_shutdown();
void cleanup();



#endif
