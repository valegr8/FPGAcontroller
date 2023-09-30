// File: TwoTasks.c

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "includes.h"
#include "altera_avalon_performance_counter.h"
#include "system.h"

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

/* Definition for the timers */
#define OVERHEAD_TIMER_TEST_SECTION 1
#define CONTEXT_SWITCH_SECTION 2

// A variable that keeps count of the first 10 measurements
int measurements_count = 0;
double measurements[10];

// A variable that keeps count of the total time
double total_time = 0;

// A variable that keeps count of the average measurement time
double avg_time = 0;

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

  while (1)
  {
    // Reset the counters
    PERF_RESET(PERFORMANCE_COUNTER_BASE);

    // Start measuring
    PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);

    // Start the timer for the context switch overhead time
    PERF_BEGIN(PERFORMANCE_COUNTER_BASE, CONTEXT_SWITCH_SECTION);

    // Unlock the other semaphore of task 1
    OSSemPost(semaphores[1]);

    // Wait for the semaphore of this task to be unlocked
    OSSemPend(semaphores[0], 0, &err);

    // Stop the timer
    PERF_STOP_MEASURING(PERFORMANCE_COUNTER_BASE);
  }
}

/* Function that calculates the average */
double avg(double *array, int n)
{
  // Variable for the sum, mean and standard deviation
  double sum = 0.0, mean, SD = 0.0;

  // Iteration variable
  int i;

  // Calculate the sum
  for (i = 0; i < n; ++i)
  {
    sum += array[i];
  }

  // Calculate the mean
  return sum / (double)n;
}

/* Function that calculates the standard deviation */
double stdev(double *array, int n)
{
  // Variable for the sum, mean and standard deviation
  double sum = 0.0, mean, SD = 0.0;

  // Iteration variable
  int i;

  // Calculate the mean
  mean = avg(array, n);

  // Calculate the standard deviation
  for (i = 0; i < n; ++i)
    SD += pow(array[i] - mean, 2);

  // Return the standard deviation
  return sqrt(SD / n);
}

/* Function that calculates the z-score */
double z_score(double measurement, double average)
{
  return (measurement - average) / stdev(measurements, 10);
}

/* Prints a message and sleeps for given time interval */
void task1(void *pdata[])
{
  // The arguments from pdata
  OS_EVENT **semaphores = (OS_EVENT **)pdata;

  // Incase of an error
  INT8U err;

  // Variable for the current measurement
  double current_measurement;

  while (1)
  {
    // Lock the semaphore of this task
    OSSemPend(semaphores[1], 0, &err);

    // Stop the timer for the context switch overhead time
    PERF_END(PERFORMANCE_COUNTER_BASE, CONTEXT_SWITCH_SECTION);

    // Get the measurement value
    current_measurement = perf_get_section_time(PERFORMANCE_COUNTER_BASE, CONTEXT_SWITCH_SECTION) / (double)alt_get_cpu_freq();

    // If there are less than 10 measurements add the current measurement to the array
    if (measurements_count < 10)
    {
      measurements[measurements_count] = current_measurement;

      // Add the time to the total time
      total_time += current_measurement;

      // Increment the total number of measurements
      measurements_count++;
    }
    else if (measurements_count == 10 & stdev(measurements, 10) < avg(measurements, 10))
    {
      // The first time 10 measurements are done check if there is an outlier by looking at the standard deviation and average
      // If there is an outlier get 10 new measurements
      measurements_count = 0;

      // Reset the total time
      total_time = 0;
    }
    else if (z_score(current_measurement, total_time / measurements_count) < 10)
    {
      // We are sure that we have 10 good measurements, check if the current measurement is an outlier

      // Add the measurement
      total_time += current_measurement;
      measurements_count++;

      // Print the average time
      printf("Average time: %.10lf\n", total_time / measurements_count);
    }

    // Reset the counters
    PERF_RESET(PERFORMANCE_COUNTER_BASE);

    // Start measuring
    PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);

    // Start the timer for the context switch overhead time
    PERF_BEGIN(PERFORMANCE_COUNTER_BASE, CONTEXT_SWITCH_SECTION);

    // Unlock the semaphore of the other task
    OSSemPost(semaphores[0]);

    // Reset the counters
    PERF_RESET(PERFORMANCE_COUNTER_BASE);

    // Start measuring
    PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);

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
  printf("Lab 3 - Context Switch\n");

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
