/* Cruise control skeleton for the IL 2206 embedded lab
 *
 * Maintainers:  Rodolfo Jordao (jordao@kth.se), George Ungereanu (ugeorge@kth.se)
 *
 * Description:
 *
 *   In this file you will find the "model" for the vehicle that is being simulated on top
 *   of the RTOS and also the stub for the control task that should ideally control its
 *   velocity whenever a cruise mode is activated.
 *
 *   The missing functions and implementations in this file are left as such for
 *   the students of the IL2206 course. The goal is that they get familiriazed with
 *   the real time concepts necessary for all implemented herein and also with Sw/Hw
 *   interactions that includes HAL calls and IO interactions.
 *
 *   If the prints prove themselves too heavy for the final code, they can
 *   be exchanged for alt_printf where hexadecimals are supported and also
 *   quite readable. This modification is easily motivated and accepted by the course
 *   staff.
 */
#include <stdio.h>
#include "system.h"
#include "includes.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "sys/alt_alarm.h"

#define DEBUG 1

#define HW_TIMER_PERIOD 100 /* 100ms */

/* Button Patterns */

#define GAS_PEDAL_FLAG 0x08
#define BRAKE_PEDAL_FLAG 0x04
#define CRUISE_CONTROL_FLAG 0x02
#define INCREASE_CRUISE_CONTROL_FLAG 0x01

/* Switch Patterns */

#define TOP_GEAR_FLAG 0x00000002
#define ENGINE_FLAG 0x00000001

/* LED Patterns */

#define LED_RED_0 0x00000001 // Engine
#define LED_RED_1 0x00000002 // Top Gear

#define LED_GREEN_1 0x0001 // Cruise Control activated
#define LED_GREEN_2 0x0002 // Cruise Control Button
#define LED_GREEN_4 0x0010 // Brake Pedal
#define LED_GREEN_6 0x0040 // Gas Pedal

/*
 * Definition of Tasks
 */

#define TASK_STACKSIZE 2048

OS_STK StartTask_Stack[TASK_STACKSIZE];
OS_STK ControlTask_Stack[TASK_STACKSIZE];
OS_STK VehicleTask_Stack[TASK_STACKSIZE];
OS_STK ButtonTask_Stack[TASK_STACKSIZE];
OS_STK SwitchTask_Stack[TASK_STACKSIZE];
OS_STK WatchdogTask_Stack[TASK_STACKSIZE];
OS_STK HelperTask_Stack[TASK_STACKSIZE];
OS_STK ExtraTask_Stack[TASK_STACKSIZE];

// Task Priorities

#define STARTTASK_PRIO 5
#define WATCHDOG_PRIO 6
#define SWITCHTASK_PRIO 8
#define BUTTONTASK_PRIO 9
#define VEHICLETASK_PRIO 10
#define CONTROLTASK_PRIO 12
#define EXTRA_WORK_PRIO 13
#define HELPER_PRIO 14

// Task Periods

#define CONTROL_PERIOD 300
#define VEHICLE_PERIOD 300
#define HYPER_PERIOD 300
#define OK_MESSAGE 1

/*
 * Definition of Kernel Objects
 */

// Mailboxes
OS_EVENT *Mbox_Throttle;
OS_EVENT *Mbox_Velocity;
OS_EVENT *Mbox_Brake;
OS_EVENT *Mbox_Engine;
OS_EVENT *Mbox_buttons;
OS_EVENT *Mbox_switches;
OS_EVENT *Mbox_Watchdog;

// Semaphores
OS_EVENT *VehicleSem; // Semaphore for the Vehicle task
OS_EVENT *ControlSem; // Semaphore for the control task
OS_EVENT *ButtonSem;  // Semaphore for the button task
OS_EVENT *SwitchSem;  // Semaphore for the switch task

// SW-Timer
OS_TMR *VehicleSWTimer; // Software Timer for the vehicle task
OS_TMR *ControlSWTimer; // Software Timer for the control task

/*
 * Types
 */
enum active
{
  on = 2,
  off = 1
};

/*
 * Global variables
 */
int delay; // Delay of HW-timer
INT16U led_green = 0;
INT32U led_red = 0;
INT16S target_velocity = 0;
INT16S MAXIMUM_VELOCITY = 80;

int *red_leds = (int *)DE2_PIO_REDLED18_BASE;
int *green_leds = (int *)DE2_PIO_GREENLED9_BASE;

/*
 * Helper functions
 */
int buttons_pressed(void)
{
  return ~IORD_ALTERA_AVALON_PIO_DATA(D2_PIO_KEYS4_BASE);
}

int switches_pressed(void)
{
  return IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_TOGGLES18_BASE);
}

/*
 * Callback functions
 */
void control_callback(OS_TMR *ptmr, void *callback_arg)
{
  // Unlock the semaphore for the control task
  OSSemPost(ControlSem);
}

void vehicle_callback(OS_TMR *ptmr, void *callback_arg)
{
  // Unlock the semaphore for the vehicle task
  OSSemPost(VehicleSem);
}

/*
 * ISR for HW Timer
 */
alt_u32 alarm_handler(void *context)
{
  OSTmrSignal(); /* Signals a 'tick' to the SW timers */

  return delay;
}

static int b2sLUT[] = {
    0x40, // 0
    0x79, // 1
    0x24, // 2
    0x30, // 3
    0x19, // 4
    0x12, // 5
    0x02, // 6
    0x78, // 7
    0x00, // 8
    0x18, // 9
    0x3F, //-
};

/*
 * convert int to seven segment display format
 */
int int2seven(int inval)
{
  return b2sLUT[inval];
}

/*
 * output current velocity on the seven segement display
 */
void show_velocity_on_sevenseg(INT8S velocity)
{
  int tmp = velocity;
  int out;
  INT8U out_high = 0;
  INT8U out_low = 0;
  INT8U out_sign = 0;

  if (velocity < 0)
  {
    out_sign = int2seven(10);
    tmp *= -1;
  }
  else
  {
    out_sign = int2seven(0);
  }

  out_high = int2seven(tmp / 10);
  out_low = int2seven(tmp - (tmp / 10) * 10);

  out = int2seven(0) << 21 |
        out_sign << 14 |
        out_high << 7 |
        out_low;
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_LOW28_BASE, out);
}

/*
 * shows the target velocity on the display
 * when the cruise control is activated (0 otherwise)
 */
void show_target_velocity(INT8U cruise_control)
{
  int tmp = (cruise_control == on) ? target_velocity : 0;
  int out;
  INT8U out_high = 0;
  INT8U out_low = 0;
  INT8U out_sign = 0;

  if (target_velocity < 0)
  {
    out_sign = int2seven(10);
    tmp *= -1;
  }
  else
  {
    out_sign = int2seven(0);
  }

  out_high = int2seven(tmp / 10);
  out_low = int2seven(tmp - (tmp / 10) * 10);

  out = int2seven(0) << 21 |
        out_sign << 14 |
        out_high << 7 |
        out_low;
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_HIGH28_BASE, out);
}

/*
 * indicates the position of the vehicle on the track with the four leftmost red LEDs
 * LEDR17: [0m, 400m)
 * LEDR16: [400m, 800m)
 * LEDR15: [800m, 1200m)
 * LEDR14: [1200m, 1600m)
 * LEDR13: [1600m, 2000m)
 * LEDR12: [2000m, 2400m]
 */
void show_position(INT16U position)
{
  // Use a switch statement to set the leds (set the bit to 1 on the position)
  switch (position)
  {
  case 0 ... 399:
    *red_leds |= 0x00020000;
    break;
  case 400 ... 799:
    *red_leds |= 0x00010000;
    break;
  case 800 ... 1199:
    *red_leds |= 0x00008000;
    break;
  case 1200 ... 1599:

    *red_leds |= 0x00004000;
    break;
  case 1600 ... 1999:
    *red_leds |= 0x00002000;
    break;
  case 2000 ... 2400:
    *red_leds |= 0x00001000;
    break;
  default:
    break;
  }
}

/*
 * The task 'VehicleTask' is the model of the vehicle being simulated. It updates variables like
 * acceleration and velocity based on the input given to the model.
 *
 * The car model is equivalent to moving mass with linear resistances acting upon it.
 * Therefore, if left one, it will stably stop as the velocity converges to zero on a flat surface.
 * You can prove that easily via basic LTI systems methods.
 */
void VehicleTask(void *pdata)
{
  // constants that should not be modified
  const unsigned int wind_factor = 1;
  const unsigned int brake_factor = 4;
  const unsigned int gravity_factor = 2;
  // variables relevant to the model and its simulation on top of the RTOS
  INT8U err;
  void *msg;
  INT8U *throttle;
  INT16S acceleration;
  INT16U position = 0;
  INT16S velocity = 0;
  enum active brake_pedal = off;
  enum active engine = off;

  printf("Vehicle task created!\n");

  while (1)
  {
    err = OSMboxPost(Mbox_Velocity, (void *)&velocity);

    // Wait until the vehicle semaphore is released
    OSSemPend(VehicleSem, 0, &err);

    /* Non-blocking read of mailbox:
       - message in mailbox: update throttle
       - no message:         use old throttle
       */
    msg = OSMboxPend(Mbox_Throttle, 1, &err);
    if (err == OS_NO_ERR)
      throttle = (INT8U *)msg;
    /* Same for the brake signal that bypass the control law */
    msg = OSMboxPend(Mbox_Brake, 1, &err);
    if (err == OS_NO_ERR)
      brake_pedal = (enum active)msg;
    /* Same for the engine signal that bypass the control law */
    msg = OSMboxPend(Mbox_Engine, 1, &err);
    if (err == OS_NO_ERR)
      engine = (enum active)msg;

    // vehichle cannot effort more than 80 units of throttle
    if (*throttle > 80)
      *throttle = 80;

    // brakes + wind
    if (brake_pedal == off)
    {
      // wind resistance
      acceleration = -wind_factor * velocity;
      // actuate with engines
      if (engine == on)
        acceleration += (*throttle);

      // gravity effects
      if (400 <= position && position < 800)
        acceleration -= gravity_factor; // traveling uphill
      else if (800 <= position && position < 1200)
        acceleration -= 2 * gravity_factor; // traveling steep uphill
      else if (1600 <= position && position < 2000)
        acceleration += 2 * gravity_factor; // traveling downhill
      else if (2000 <= position)
        acceleration += gravity_factor; // traveling steep downhill
    }
    // if the engine and the brakes are activated at the same time,
    // we assume that the brake dynamics dominates, so both cases fall
    // here.
    else
      acceleration = -brake_factor * velocity;

    // printf("Position: %d m\n", position);
    // printf("Velocity: %d m/s\n", velocity);
    // printf("Accell: %d m/s2\n", acceleration);
    // printf("Throttle: %d V\n", *throttle);

    position = position + velocity * VEHICLE_PERIOD / 1000;
    velocity = velocity + acceleration * VEHICLE_PERIOD / 1000.0;
    // reset the position to the beginning of the track
    if (position > 2400)
      position = 0;

    show_velocity_on_sevenseg((INT8S)velocity);
  }
}

/*
 * The task 'ControlTask' is the main task of the application. It reacts
 * on sensors and generates responses.
 */
void ControlTask(void *pdata)
{
  INT8U err;
  INT8U STATIONARY_THROTTLE = 40;
  INT8U throttle = STATIONARY_THROTTLE; /* Value between 0 and 80, which is interpreted as between 0.0V and 8.0V */
  int temp_throttle = 0;
  void *msg;
  INT16S *current_velocity;
  INT16U position = 0;

  enum active gas_pedal = off;
  enum active top_gear = off;
  enum active cruise_control = off;

  printf("Control Task created!\n");

  // variable that holds the messages from buttons and switches
  INT8U msg_buttons = 0;
  int msg_switches = 0;
  INT8U engine_state = 0;

  // Constants and variables for the cruise control using a PID controller
  float KP = 0.5;
  float KI = 1;
  float KD = 0;
  float error = 0;      // Error term between desired and current velocity
  float integral = 0;   // Integral term of errors
  float last_error = 0; // Last error term
  float derivative = 0; // Derivative term of the last error

  // Base pointers for the leds
  *red_leds = 0;
  *green_leds = 0;

  while (1)
  {
    msg = OSMboxPend(Mbox_Velocity, 0, &err);
    current_velocity = (INT16S *)msg;

    // Unlock the semaphores for the button and switch tasks
    OSSemPost(ButtonSem);
    OSSemPost(SwitchSem);

    // Wait until the button and switch tasks are done
    msg = OSMboxPend(Mbox_buttons, 1, &err);
    msg_buttons = (INT8U)((err == OS_NO_ERR) ? ((INT8U *)msg) : 0);
    msg = OSMboxPend(Mbox_switches, 1, &err);
    msg_switches = (int)((err == OS_NO_ERR) ? ((int *)msg) : 0);

    // Here you can use whatever technique or algorithm that you prefer to control
    // the velocity via the throttle. There are no right and wrong answer to this controller, so
    // be free to use anything that is able to maintain the cruise working properly. You are also
    // allowed to store more than one sample of the velocity. For instance, you could define
    //
    // INT16S previous_vel;
    // INT16S pre_previous_vel;
    // ...
    //
    // If your control algorithm/technique needs them in order to function.

    /* Logic for handling the engine,
    turning it on and off
    If the velocity is zero and the car is not moving then the car is turned off
    */
    if (((msg_switches & ENGINE_FLAG) == 0) & (engine_state == 1))
    {
      // If there is still velocity then set the throttle to 0
      if (*current_velocity != 0)
        throttle = 0;
      else
      {
        // Send a message to the engine mailbox
        err = OSMboxPost(Mbox_Engine, (void *)off);

        // Set the state of the engine to 0
        engine_state = 0;

        printf("Turned off engine\n");
      }
    }

    // Turn the engine on, make sure the queue is not full
    else if (((msg_switches & ENGINE_FLAG) == 1) & (engine_state == 0))
    {
      // Send a message to the engine mailbox that it is on
      err = OSMboxPost(Mbox_Engine, (void *)on);
      if (err == OS_ERR_NONE)
      {
        engine_state = 1;
        printf("Turned on engine\n");

        // Send a message to turn off the brake
        err = OSMboxPost(Mbox_Brake, (void *)off);
      }
    }

    /*
    Get information about the gas pedal, top gear and cruise control
    */

    gas_pedal = (msg_buttons & GAS_PEDAL_FLAG) ? on : off;
    top_gear = (msg_switches & TOP_GEAR_FLAG) ? on : off;

    if ((msg_buttons & CRUISE_CONTROL_FLAG) && (*current_velocity > 20) && (top_gear == on))
    {
      target_velocity = *current_velocity % MAXIMUM_VELOCITY;
      cruise_control = on;
    }

    // Turn cruise control off if the brake is pressed or the gas is pressed
    if ((msg_buttons & (BRAKE_PEDAL_FLAG | GAS_PEDAL_FLAG)) |
        ((cruise_control == on) & (top_gear == off)) |
        (*current_velocity < 25))
      cruise_control = off;

    /*
    Logic for handling the break, gas and cruise control. It also is of this order in importance
    Starts out with having a throttle of 40 and then changing it according to the rules
    */
    throttle = STATIONARY_THROTTLE;

    if (msg_buttons & BRAKE_PEDAL_FLAG)
      throttle = 0;
    else if (msg_buttons & GAS_PEDAL_FLAG)
      throttle = 80;
    else if (cruise_control == on)
    {
      // The error term (difference between target and current velocity)
      error = target_velocity - *current_velocity;

      // The integral term (all the errors so far)
      integral += error;

      // The derivative term (the difference between the last error and the current error)
      derivative = error - derivative;
      last_error = error;

      // Calculate the throttle which is at least 0 and at most 80
      temp_throttle = throttle + KP * error + KI * integral + KD * derivative;

      // Make sure the throttle is between 0 and 80
      if (temp_throttle > 80)
        throttle = 80;
      else if (temp_throttle < 0)
        throttle = 0;
      else
        throttle = temp_throttle;
    }

    // Send the throttle and break
    err = OSMboxPost(Mbox_Throttle, (void *)&throttle);
    err = OSMboxPost(Mbox_Brake, (void *)(msg_buttons & BRAKE_PEDAL_FLAG) ? on : off);

    // Set the green leds according to the buttons pressed
    // The cruise control can be on even though the button is not pressed
    // therefore the flag is used if it is on

    /* Turn on the green lights, start with the inputs */
    *green_leds = msg_buttons;

    // If the cruise control is on then turn on the cruise control light (1)
    *green_leds += (cruise_control == on) ? LED_GREEN_1 : 0;

    /* turn on the red lights */
    *red_leds = 0;

    // If the motor is on then turn on the engine light (0)
    *red_leds += (engine_state == 1) ? LED_RED_0 : 0;

    // If the car is in top gear then turn on the top gear light (1)
    *red_leds += (top_gear == on) ? LED_RED_1 : 0;

    // Turn on leds 4 to 9 if they are pressed using a mask
    *red_leds += (0x3F << 4) & msg_switches;

    // Calculate the new position mod 2400
    position = (position + *current_velocity * CONTROL_PERIOD / 1000) % 2400;

    // Show the position
    show_position(position);

    // Show the target velocity
    show_target_velocity(cruise_control);

    // Wait until the vehicle semaphore is released
    OSSemPend(ControlSem, 0, &err);
  }
}

/*
 * Task that creates the signals ENGINE and TOP_GEAR periodically
 */
void SwitchIO()
{
  printf("SwitchIO created!\n");

  INT8U err;

  while (1)
  {
    // Wait for the semaphore
    OSSemPend(SwitchSem, 0, &err);

    // Set the values of the red leds
    led_red = switches_pressed();
    OSMboxPost(Mbox_switches, (void *)(led_red));
  }
}

/*
 * Task that creates the signals CRUISE_CONTROL, BRAKE_PEDAL and GAS_PEDAL periodically
 */
void ButtonIO(void *pdata)
{
  printf("ButtonIO created!\n");

  INT8U err;

  while (1)
  {
    // Wait for the semaphore
    OSSemPend(ButtonSem, 0, &err);

    // set the values of the buttons
    led_green = buttons_pressed();

    // PRIORITY SCHEME : BREAKING > GAS > CRUISE CONTROL
    if (led_green & BRAKE_PEDAL_FLAG)
    {
      OSMboxPost(Mbox_buttons, (void *)BRAKE_PEDAL_FLAG);
    }
    else if (led_green & GAS_PEDAL_FLAG)
    {
      OSMboxPost(Mbox_buttons, (void *)GAS_PEDAL_FLAG);
    }
    else if (led_green & CRUISE_CONTROL_FLAG)
    {
      OSMboxPost(Mbox_buttons, (void *)CRUISE_CONTROL_FLAG);
    }
    // If the increase cruise controll button is pressed then increase the target velocity
    if (led_green & INCREASE_CRUISE_CONTROL_FLAG)
      target_velocity = (target_velocity + 1) % MAXIMUM_VELOCITY;
  }

  OSTaskDel(OS_PRIO_SELF);
}

/* Task for the watchdog timer to detect whteher the system is overloaded
 Receives messages from the helper task.
 Receives OK if the system is not overloaded
 If no OK is received and the timer expires then send a warning that the system is overloaded*/
void watchdog_task(void *pdata)
{
  INT8U err;
  void *msg;

  printf("Watchdog task created!\n");

  while (1)
  {
    // Wait for the helper task to send a message
    msg = OSMboxPend(Mbox_Watchdog, HYPER_PERIOD, &err);
    if (err == OS_TIMEOUT)
      printf("WARNING: The system is overloaded\n");
  }
  return;
}

/* Helper task that notifies the watchdog task if it is able to do something */
void helper_task(void *pdata)
{
  printf("Helper task created!\n");
  while (1)
  {
    OSMboxPost(Mbox_Watchdog, OK_MESSAGE);
  }
}

/* Task that does extra work depending on switches SW4 to SW9 which is interpreted as a binary number */
void extra_task(void *pdata)
{
  INT32U extra_work = 0;
  int dummy_var = 0;
  INT32U start_time = 0;
  int working_time = 0;

  printf("Extra task created!\n");

  while (1)
  {
    // Get the value of the switches
    extra_work = switches_pressed();

    // Get the binary value from switch 4 to 9
    extra_work = extra_work >> 4;

    // Mask it to extract only the last 6 bits
    extra_work = extra_work & 0x3F;

    // Have the value at most 50 and then multiply it by 2 to get the percentage
    extra_work = (extra_work > 50) ? 100 : extra_work * 2;

    // Calculate the working
    working_time = (extra_work / 100) * HYPER_PERIOD;

    // If the switches are not 0 then do extra work
    if (extra_work != 0)
      // TODO: do extra work for working_time ms
      start_time = OSTimeGet(); // Get the current time

    while (OSTimeGet() - start_time < working_time)
    {
      // Simulate extra work
      dummy_var++; // Dummy computation to spend time :)
    }

    // Delay for a bit of time so the other task can do its thing
    OSTimeDlyHMSM(0, 0, 0, 100);
  }
}

/*
 * The task 'StartTask' creates all other tasks kernel objects and
 * deletes itself afterwards.
 */
void StartTask(void *pdata)
{
  INT8U err;
  void *context;

  static alt_alarm alarm; /* Is needed for timer ISR function */

  /* Base resolution for SW timer : HW_TIMER_PERIOD ms */
  delay = alt_ticks_per_second() * HW_TIMER_PERIOD / 1000;
  printf("delay in ticks %d\n", delay);

  /*
   * Create Hardware Timer with a period of 'delay'
   */
  if (alt_alarm_start(&alarm,
                      delay,
                      alarm_handler,
                      context) < 0)
  {
    printf("No system clock available!n");
  }

  /*
   * Create both semaphores
   */
  VehicleSem = OSSemCreate(0);
  ControlSem = OSSemCreate(0);
  ButtonSem = OSSemCreate(0);
  SwitchSem = OSSemCreate(0);

  /*
   * Create and start both Software Timers
   */

  ControlSWTimer = OSTmrCreate(
      0,
      CONTROL_PERIOD / HW_TIMER_PERIOD,
      OS_TMR_OPT_PERIODIC,
      (OS_TMR_CALLBACK)control_callback,
      NULL,
      NULL,
      &err);

  OSTmrStart(ControlSWTimer, &err);

  VehicleSWTimer = OSTmrCreate(0,
                               VEHICLE_PERIOD / HW_TIMER_PERIOD,
                               OS_TMR_OPT_PERIODIC,
                               (OS_TMR_CALLBACK)vehicle_callback,
                               NULL,
                               NULL,
                               &err);

  OSTmrStart(VehicleSWTimer, &err);

  /*
   * Creation of Kernel Objects
   */

  // Mailboxes
  Mbox_Throttle = OSMboxCreate((void *)0); /* Empty Mailbox - Throttle */
  Mbox_Velocity = OSMboxCreate((void *)0); /* Empty Mailbox - Velocity */
  Mbox_Brake = OSMboxCreate((void *)1);    /* Empty Mailbox - Velocity */
  Mbox_Engine = OSMboxCreate((void *)1);   /* Empty Mailbox - Engine */
  Mbox_buttons = OSMboxCreate((void *)0);  /* Empty Mailbox - Buttons */
  Mbox_switches = OSMboxCreate((void *)0); /* Empty Mailbox - Switches */
  Mbox_Watchdog = OSMboxCreate((void *)0); /* Empty Mailbox - Watchdog */

  /*
   * Create statistics task
   */

  OSStatInit();

  /*
   * Creating Tasks in the system
   */

  err = OSTaskCreateExt(
      ControlTask, // Pointer to task code
      NULL,        // Pointer to argument that is
      // passed to task
      &ControlTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      CONTROLTASK_PRIO,
      CONTROLTASK_PRIO,
      (void *)&ControlTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
      VehicleTask, // Pointer to task code
      NULL,        // Pointer to argument that is
      // passed to task
      &VehicleTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      VEHICLETASK_PRIO,
      VEHICLETASK_PRIO,
      (void *)&VehicleTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
      SwitchIO, // Pointer to task code
      NULL,     // Pointer to argument that is
      // passed to task
      &SwitchTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      SWITCHTASK_PRIO,
      SWITCHTASK_PRIO,
      (void *)&SwitchTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
      ButtonIO, // Pointer to task code
      NULL,     // Pointer to argument that is
      // passed to task
      &ButtonTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      BUTTONTASK_PRIO,
      BUTTONTASK_PRIO,
      (void *)&ButtonTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
      watchdog_task, // Pointer to task code
      NULL,          // Pointer to argument that is
      // passed to task
      &WatchdogTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      WATCHDOG_PRIO,
      WATCHDOG_PRIO,
      (void *)&WatchdogTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
      helper_task, // Pointer to task code
      NULL,        // Pointer to argument that is
      // passed to task
      &HelperTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      HELPER_PRIO,
      HELPER_PRIO,
      (void *)&HelperTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
      extra_task, // Pointer to task code
      NULL,       // Pointer to argument that is
      // passed to task
      &ExtraTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      EXTRA_WORK_PRIO,
      EXTRA_WORK_PRIO,
      (void *)&ExtraTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK);

  printf("All Tasks and Kernel Objects generated!\n");

  /* Task deletes itself */

  OSTaskDel(OS_PRIO_SELF);
}

/*
 *
 * The function 'main' creates only a single task 'StartTask' and starts
 * the OS. All other tasks are started from the task 'StartTask'.
 *
 */
int main(void)
{
  printf("Lab: Cruise Control\n");

  OSTaskCreateExt(
      StartTask, // Pointer to task code
      NULL,      // Pointer to argument that is
      // passed to task
      (void *)&StartTask_Stack[TASK_STACKSIZE - 1], // Pointer to top
      // of task stack
      STARTTASK_PRIO,
      STARTTASK_PRIO,
      (void *)&StartTask_Stack[0],
      TASK_STACKSIZE,
      (void *)0,
      OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
  OSStart();

  return 0;
}
