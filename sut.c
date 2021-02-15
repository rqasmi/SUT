#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "sut.h"
#include "queue.h"
#include "io.h"

pthread_t c_exec_handle;
pthread_t i_exec_handle;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; // A lock to prevent data race
threaddesc *threadarr[MAX_THREADS];
struct queue ready_queue;
struct queue io_queue;
open_message o_msg;
write_message w_msg;
ucontext_t c, i, curr_context; // c-exec, i-exec and current user thread contexts
char read_buf[1024];
int num_threads = 0, exited_threads = 0;
bool yield_flag = false, io_flag = false, read_flag = false, write_flag = false, close_flag = false;

void lock_and_insert_entry(struct queue *q, struct queue_entry *e);

/**
 * This function runs the C-EXEC thread. 
 *
**/
void *c_exec(void *arg)
{
  while (true)
  {
    pthread_mutex_lock(&m);
    struct queue_entry *entry = queue_pop_head(&ready_queue);
    pthread_mutex_unlock(&m);
    if (entry)
    {
      ucontext_t t = ((threaddesc *)(entry->data))->threadcontext;
      usleep(1000 * 1000);
      swapcontext(&c, &t);
      ((threaddesc *)(entry->data))->threadcontext = curr_context;
      if (yield_flag)
      {
        lock_and_insert_entry(&ready_queue, entry);
        yield_flag = false;
      }
      else if (read_flag || write_flag || io_flag || close_flag)
      {
        lock_and_insert_entry(&io_queue, entry);
      }
    }
    else
    {
      usleep(1000 * 1000);
    }

    pthread_testcancel();
  }
}

/**
 * This function runs the I-EXEC thread.
 * @params: arg
 * @return: void
 * 
**/
void *i_exec(void *arg)
{
  int sockfd;
  getcontext(&i);
  while (true)
  {
    pthread_mutex_lock(&m);
    struct queue_entry *entry = queue_pop_head(&io_queue);
    pthread_mutex_unlock(&m);
    if (entry)
    {
      if (io_flag)
      {
        io_flag = false;
        connect_to_server(o_msg.buf, o_msg.port, &sockfd);
        printf("Opened connection\n");
      }
      else if (read_flag)
      {
        read_flag = false;
        recv_message(sockfd, read_buf, sizeof(read_buf));
        printf("Read\n");
      }
      else if (write_flag)
      {
        write_flag = false;
        send_message(sockfd, w_msg.buf, w_msg.size);
        printf("Wrote\n");
      }
      else if (close_flag)
      {
        close_flag = false;
        close_socket(sockfd);
        printf("Closed\n");
      }
      lock_and_insert_entry(&ready_queue, entry);
    }

    pthread_testcancel();
  }
}

/**
 * This function initializes the ready_queue and the io_queue. It then creates c-exec 
 * and i-exec threads.
 *  
**/
void sut_init()
{
  ready_queue = queue_create();
  queue_init(&ready_queue);

  io_queue = queue_create();
  queue_init(&io_queue);

  pthread_create(&c_exec_handle, NULL, c_exec, NULL);
  pthread_create(&i_exec_handle, NULL, i_exec, NULL);

}

/**
 * This function creates user tasks wrapped in a threaddesc struct object. It increments the number 
 * of threads on each call.
 * 
  * @params: fn: The function to for creating the task.
  * @return: boolean indicating if the thread had been created or not. If num_threads has reached the maximum number of threads allowed
  * then no thread is created and false is retuned, otherwise true.
 * 
**/
bool sut_create(sut_task_f fn)
{

  threaddesc *tdescptr = (threaddesc *)malloc(sizeof(threaddesc));

  if (num_threads >= MAX_THREADS)
  {
    printf("FATAL: Maximum thread limit reached... creation failed! \n");
    return false;
  }

  threadarr[num_threads] = tdescptr;

  struct queue_entry *node = queue_new_node(tdescptr);
  pthread_mutex_lock(&m);
  queue_insert_tail(&ready_queue, node);
  pthread_mutex_unlock(&m);

  getcontext(&(tdescptr->threadcontext));
  tdescptr->threadid = num_threads;
  tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
  tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
  tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
  tdescptr->threadcontext.uc_link = 0;
  tdescptr->threadcontext.uc_stack.ss_flags = 0;
  tdescptr->threadfunc = fn;

  makecontext(&(tdescptr->threadcontext), fn, 0);

  num_threads++;

  return true;
}

/**
 * This function sets the yield_flag to true and switches context to C-EXEC.
 * @params: void
 * @return: void
 * 
**/
void sut_yield()
{
  yield_flag = true;
  swapcontext(&curr_context, &c);
}

/**
 * This function increments the number of exited threads and switches context to C-EXEC.
 * @params: void
 * @return: void
 * 
**/

void sut_exit()
{
  exited_threads++;
  yield_flag = false;
  swapcontext(&curr_context, &c);
}

/**
 * This function cancels the pthreads (C-EXEC and I-EXEC) when the queue is empty
 * and the number of exited threads has reached the number of created threads.
 * @params: void
 * @return: void
 * 
**/

void sut_shutdown()
{
  usleep(1000);
  while (true)
  {
    pthread_mutex_lock(&m);
    if (queue_peek_front(&ready_queue) == NULL && exited_threads == num_threads)
    {
      cleanup();
      pthread_cancel(c_exec_handle);
      pthread_cancel(i_exec_handle);
      break;
    }
    pthread_mutex_unlock(&m);
    usleep(100);
  }
  pthread_mutex_unlock(&m);
  printf("Shutdown\n");

  pthread_join(c_exec_handle, NULL);
  pthread_join(i_exec_handle, NULL);
}

/**
 * This function copies the parameters to an o_message struct and switches context to C-EXEC.
 * @params: 
 * dest: The destination process
 * port: The port number
 * @return: void
 * 
**/
void sut_open(char *dest, int port)
{
  io_flag = true; 
  strcpy(o_msg.buf, dest);
  o_msg.port = port;

  swapcontext(&curr_context, &c);
}

/**
 * This function switches context to C-EXEC and returns the read buffer.
 * @params: void.
 * @return: The buffer.
 * 
**/

char *sut_read()
{
  read_flag = true;
  swapcontext(&curr_context, &c);
  return read_buf;
}


/**
 * This function copies the parameters to a w_message struct and switches context to 
 * C-EXEC.
 * @params: 
 * dest: The buffer to send.
 * port: The size of the buffer.
 * @return: void
 * 
**/

void sut_write(char *buf, int size)
{
  write_flag = true;
  strcpy(w_msg.buf, buf);
  w_msg.size = size;
  swapcontext(&curr_context, &c);
}

/**
 * This function closes the socket connection.
 * @params: void
 * @return: void
 * 
**/
void sut_close() {
  close_flag = true;
  swapcontext(&curr_context, &c);
}

/**
 * This function attempts to insert into the queue with a lock.
 * @params:
 * q: The queue to insert into.
 * e: The entry to insert.
 * @return: void
 **/

void lock_and_insert_entry(struct queue *q, struct queue_entry *e)
{
  pthread_mutex_lock(&m);
  queue_insert_tail(q, e);
  pthread_mutex_unlock(&m);
}

/**
 * This function frees the allocated memory.
 * @params: void
 * @return: void
 **/
void cleanup() {
  for(int i = 0; i < num_threads; i++) {
    free(threadarr[i]);
  }
}