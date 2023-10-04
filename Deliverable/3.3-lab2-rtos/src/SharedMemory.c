// File: TwoTasks.c

#include <stdio.h>
#include "includes.h"
#include <string.h>

#define DEBUG 0

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
#define TASK_STACKSIZE 2048
OS_STK task1_stk[TASK_STACKSIZE]; // OS_STK makes a stack type
OS_STK task2_stk[TASK_STACKSIZE];
OS_STK stat_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define TASK1_PRIORITY 6 // highest priority
#define TASK2_PRIORITY 7
#define TASK_STAT_PRIORITY 12 // lowest priority

// Struct for the arguments
typedef struct
{
  OS_EVENT *semaphores[2];
  int *int_shared_ptr;
} TaskArguments;

// Create a global variable of the arguments
TaskArguments arguments;

void printStackSize(char *name, INT8U prio)
{
  INT8U err;
  OS_STK_DATA stk_data;

  err = OSTaskStkChk(prio, &stk_data);
  if (err == OS_NO_ERR)
  {
    if (DEBUG == 1)
      printf("%s (priority %d) - Used: %d; Free: %d\n",
             name, prio, stk_data.OSUsed, stk_data.OSFree);
  }
  else
  {
    if (DEBUG == 1)
      printf("Stack Check Error!\n");
  }
}

/* Prints a message and sleeps for given time interval */
void task0(void *pdata[])
{
  // Incase of an error
  INT8U err;

  // Integer that keeps incrementing
  int i = 1;
  while (1)
  {
    // Put the integer in the shared memory
    *arguments.int_shared_ptr = i;

    // Print the number that is put in the shared memory
    printf("Sending: %d\n", i);

    // Unlock the other semaphore of task 1
    OSSemPost(arguments.semaphores[1]);

    // Wait for the semaphore of this task to be unlocked
    OSSemPend(arguments.semaphores[0], 0, &err);

    // Print the state of the task
    printf("Receiving:  %d\n", *arguments.int_shared_ptr);

    // Increment the state by 1
    i++;
  }
}

/* Prints a message and sleeps for given time interval */
void task1(void *pdata[])
{
  // Incase of an error
  INT8U err;

  while (1)
  {
    // Lock the semaphore of this task
    OSSemPend(arguments.semaphores[1], 0, &err);

    // Multiply the value at the shared pointer by -1
    *arguments.int_shared_ptr *= -1;

    // Unlock the semaphore of the other task
    OSSemPost(arguments.semaphores[0]);
  }
}

/* Printing Statistics */
void statisticTask(void *pdata)
{
  while (1)
  {
    printStackSize("Task1", TASK1_PRIORITY);
    printStackSize("Task2", TASK2_PRIORITY);
    printStackSize("StatisticTask", TASK_STAT_PRIORITY);
  }
}

/* The main function creates two task and starts multi-tasking */
int main(void)
{
  printf("Lab 3 - Two Tasks\n");

  // An integer and its pointer
  INT8U int_shared = 0;
  INT8U *int_shared_ptr = &int_shared;

  // Create one argument to pass both semaphores as well as the shared integer memory location
  arguments = (TaskArguments){
      .semaphores = {OSSemCreate(0), OSSemCreate(0)},
      .int_shared_ptr = int_shared_ptr};

  OSTaskCreateExt(task0,                          // Pointer to task code
                  NULL,                           // Pointer to argument passed to task
                  &task1_stk[TASK_STACKSIZE - 1], // Pointer to top of task stack
                  TASK1_PRIORITY,                 // Desired Task priority
                  TASK1_PRIORITY,                 // Task ID
                  &task1_stk[0],                  // Pointer to bottom of task stack
                  TASK_STACKSIZE,                 // Stacksize
                  NULL,                           // Pointer to user supplied memory (not needed)
                  OS_TASK_OPT_STK_CHK |           // Stack Checking enabled
                      OS_TASK_OPT_STK_CLR         // Stack Cleared
  );

  OSTaskCreateExt(task1,                          // Pointer to task code
                  NULL,                           // Pointer to argument passed to task
                  &task2_stk[TASK_STACKSIZE - 1], // Pointer to top of task stack
                  TASK2_PRIORITY,                 // Desired Task priority
                  TASK2_PRIORITY,                 // Task ID
                  &task2_stk[0],                  // Pointer to bottom of task stack
                  TASK_STACKSIZE,                 // Stacksize
                  NULL,                           // Pointer to user supplied memory (not needed)
                  OS_TASK_OPT_STK_CHK |           // Stack Checking enabled
                      OS_TASK_OPT_STK_CLR         // Stack Cleared
  );

  if (DEBUG == 1)
  {
    OSTaskCreateExt(statisticTask,                 // Pointer to task code
                    NULL,                          // Pointer to argument passed to task
                    &stat_stk[TASK_STACKSIZE - 1], // Pointer to top of task stack
                    TASK_STAT_PRIORITY,            // Desired Task priority
                    TASK_STAT_PRIORITY,            // Task ID
                    &stat_stk[0],                  // Pointer to bottom of task stack
                    TASK_STACKSIZE,                // Stacksize
                    NULL,                          // Pointer to user supplied memory (not needed)
                    OS_TASK_OPT_STK_CHK |          // Stack Checking enabled
                        OS_TASK_OPT_STK_CLR        // Stack Cleared
    );
  }

  OSStart();
  return 0;
}
