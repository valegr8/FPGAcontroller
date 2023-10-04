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
  // The arguments from pdata
  OS_EVENT **semaphores = (OS_EVENT **)pdata;

  // Incase of an error
  INT8U err;

  // The state of the task
  int state = 0;
  while (1)
  {
    // Print the initial state
    printf("task 0 - state %d\n", state);

    // Increment the state
    state++;

    // Unlock the other semaphore of task 1
    OSSemPost(semaphores[1]);

    // Wait for the semaphore of this task to be unlocked
    OSSemPend(semaphores[0], 0, &err);

    // Print the state of the task
    printf("task 0 - state %d\n", state);

    // Decrement the state of the task by 1
    state--;
  }
}

/* Prints a message and sleeps for given time interval */
void task1(void *pdata[])
{
  // The arguments from pdata
  OS_EVENT **semaphores = (OS_EVENT **)pdata;

  // Incase of an error
  INT8U err;

  // The state
  int state = 0;

  while (1)
  {
    // Lock the semaphore of this task
    OSSemPend(semaphores[1], 0, &err);

    // Print the state of the task
    printf("task 1 - state %d\n", state);

    // increment the state by 1
    state++;

    // Print the state of the task
    printf("task 1 - state %d\n", state);

    // Decrement the state of the task by 1
    state--;

    // Unlock the semaphore of the other task
    OSSemPost(semaphores[0]);
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

  // Create one argument to pass both semaphores as well as the shared integer memory location
  OS_EVENT *arguments[2] = {OSSemCreate(0), OSSemCreate(0)};

  OSTaskCreateExt(task0,                          // Pointer to task code
                  &arguments,                     // Pointer to argument passed to task
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
                  &arguments,                     // Pointer to argument passed to task
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
