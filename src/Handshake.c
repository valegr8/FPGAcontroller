// File: Handshake.c
// Author: Valeria Grotto 

#include <stdio.h>
#include "includes.h" //the file should be in the same folder
#include <string.h>

#define DEBUG 0

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
#define   TASK_STACKSIZE       2048
OS_STK    task1_stk[TASK_STACKSIZE];
OS_STK    task2_stk[TASK_STACKSIZE];
OS_STK    stat_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define TASK1_PRIORITY      6  // highest priority
#define TASK2_PRIORITY      7
#define TASK_STAT_PRIORITY 12  // lowest priority 


//define 2 semaphores as global variables
OS_EVENT *s1;
OS_EVENT *s2;


void printStackSize(char* name, INT8U prio) 
{
  INT8U err;
  OS_STK_DATA stk_data;
    
  err = OSTaskStkChk(prio, &stk_data);
  if (err == OS_NO_ERR) {
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
void task1(void* pdata)
{
  INT8U err;
  //task state
  int state = 0;  
  int k = 0;
  while (1) //k<3)
  {
    k++;

     //use semaphore -> Wait statement, the first time it can execute since s1 is initialized to 1
     OSSemPend(s1,0, &err); //as argumet I pass: - the pointer to the created sem, the timer setted to 0 -> it waits indefinitely, an error pointer 
     printf("Task 0 - State %d\n", state);
    
      //Signal to T2
      err = OSSemPost(s2);
      switch (err) {
        case OS_ERR_SEM_OVF:
          /* Semaphore has overflowed */
         printf("[T1] Semaphore Error\n"); 
         break; 
      }
      
      //Waits until T2 gives a signal
      OSSemPend(s1, 0, &err);  
      
     state = 1;
     printf("Task 0 - State %d\n", state);
     state = 0;
     printf("Task 0 - State %d\n", state);
    
      //Signal to T2
      err = OSSemPost(s2);
      switch (err) {
        case OS_ERR_SEM_OVF:
          /* Semaphore has overflowed */
         printf("[T1] Semaphore Error\n"); 
         break; 
      }
      
   }
}

/* Prints a message and sleeps for given time interval */
void task2(void* pdata)
{
  INT8U err;
  //task state
  int state = 0;  
 
  int k=0;
  while (1) //k<3)
  {
    //use semaphore -> Waits T1
     OSSemPend(s2,0, &err); //as argumet I pass: - the pointer to the created sem, the timer setted to 0 -> it waits indefinitely, an error pointer 
     printf("Task 1 - State %d\n", state);
     state = 1;
     printf("Task 1 - State %d\n", state);
    
      //Signal to T1
      err = OSSemPost(s1);
      switch (err) {
        case OS_ERR_SEM_OVF:
          /* Semaphore has overflowed */
         printf("[T2] Semaphore Error\n"); 
         break; 
      }
      
      //Waits until T1 gives a signal
      OSSemPend(s2, 0, &err);  
      
     state = 0;
     printf("Task 1 - State %d\n", state);
    
      //Signal to T1
      err = OSSemPost(s1);
      switch (err) {
        case OS_ERR_SEM_OVF:
          /* Semaphore has overflowed */
         printf("[T1] Semaphore Error\n"); 
         break; 
      }
    k++;    
  }
}

/* Printing Statistics */
void statisticTask(void* pdata)
{
  while(1)
    {
      printStackSize("Task1", TASK1_PRIORITY);
      printStackSize("Task2", TASK2_PRIORITY);
      printStackSize("StatisticTask", TASK_STAT_PRIORITY);
    }
}

/* The main function creates two task and starts multi-tasking */
int main(void)
{

  //Create semaphores
   s1 = OSSemCreate(1); //this statement creates a semaphore with initial value set to 1, it returns the sem control bklock (or null if not available)
   s2 = OSSemCreate(0); //initialized to 0 so that T1 executes first
  

  printf("\n------------------Handshake--------------------\n");

  OSTaskCreateExt
    ( task1,                        // Pointer to task code
      NULL,                         // Pointer to argument passed to task -> here I pass the pointer to the SCB
      &task1_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
      TASK1_PRIORITY,               // Desired Task priority
      TASK1_PRIORITY,               // Task ID
      &task1_stk[0],                // Pointer to bottom of task stack
      TASK_STACKSIZE,               // Stacksize
      NULL,                         // Pointer to user supplied memory (not needed)
      OS_TASK_OPT_STK_CHK |         // Stack Checking enabled 
      OS_TASK_OPT_STK_CLR           // Stack Cleared                                 
      );
	   
  OSTaskCreateExt
    ( task2,                        // Pointer to task code
      NULL,                         // Pointer to argument passed to task
      &task2_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
      TASK2_PRIORITY,               // Desired Task priority
      TASK2_PRIORITY,               // Task ID
      &task2_stk[0],                // Pointer to bottom of task stack
      TASK_STACKSIZE,               // Stacksize
      NULL,                         // Pointer to user supplied memory (not needed)
      OS_TASK_OPT_STK_CHK |         // Stack Checking enabled 
      OS_TASK_OPT_STK_CLR           // Stack Cleared                       
      );  

  if (DEBUG == 1)
    {
      OSTaskCreateExt
	( statisticTask,                // Pointer to task code
	  NULL,                         // Pointer to argument passed to task
	  &stat_stk[TASK_STACKSIZE-1],  // Pointer to top of task stack
	  TASK_STAT_PRIORITY,           // Desired Task priority
	  TASK_STAT_PRIORITY,           // Task ID
	  &stat_stk[0],                 // Pointer to bottom of task stack
	  TASK_STACKSIZE,               // Stacksize
	  NULL,                         // Pointer to user supplied memory (not needed)
	  OS_TASK_OPT_STK_CHK |         // Stack Checking enabled 
	  OS_TASK_OPT_STK_CLR           // Stack Cleared                              
	  );
    }  

  OSStart();
  return 0;
}
