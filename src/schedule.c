/*
 * schedule.c
 *
 *  Created on: Jun 29, 2024
 *      Author: anton
 */

#include "schedule.h"

static sched_slot sched_array[SCHED_MAX];
extern volatile unsigned long ticks;


unsigned long get_Time(void) {
	return ticks;
}


int sched_insert(int code, void (*func)(void), unsigned int x_timer)
{
   int i;
   for (i = 0; i < SCHED_MAX; i++)
	  if (sched_array[i].code == SCHED_NOTUSED)
		 break;
   if (i == SCHED_MAX)
   {
//        print_allarm("SCHEDULER FULL", NOALARM);
	  //ERROR(0);
	  return(0);
   }
   else
   if (x_timer > 0x80000000L)
   {
//        print_allarm("SCHEDULER TIMERR", NOALARM);
	  //ERROR(0);
	  return(0);
   }
   else
   {
	  sched_array[i].code = code;
	  sched_array[i].func = func;
	  sched_array[i].time = get_Time() + x_timer;
	  sched_array[i].period = x_timer;
	  //printf("\nscheduled = code: %d, x_timer: %lu", code, ticks);
	  return(1);
   }
}

int sched_del_all_func(void)
{
   int i;
   for (i = 0; i < SCHED_MAX; i++)
	  sched_array[i].code = SCHED_NOTUSED;

	return(1);
}

int sched_find_func(void (*func)(void))
{
   int i;
   for (i = 0; i < SCHED_MAX; i++)
	  if (sched_array[i].code != SCHED_NOTUSED)
		 if (func == sched_array[i].func)
			return(i);
   return(-1);
}

int sched_del_by_func(void (*func)(void))
{
   int i;
   if ((i = sched_find_func(func)) >= 0)
	  return(sched_delete(i));
   else
	  return(0);
}

void sched_rep_by_func(int code, void (*func)(void), unsigned int x_timer)
{
   sched_del_by_func(func);
   sched_insert(code, func, (long) x_timer);
}

void sched_rep_time_by_func(void (*func)(void), unsigned int x_timer)
{
   int i;
   if ((i = sched_find_func(func)) >= 0)
	  sched_array[i].time = get_Time()+ x_timer;

}

int sched_delete(int slot)
{
   if (sched_array[slot].code == SCHED_NOTUSED)
   {
//	  ERROR(0);
//       print_allarm("DEL NOTUSED SCHED", NOALARM);
	  return(0);
   }
   else
   {
	  sched_array[slot].code = SCHED_NOTUSED;
	  return(1);
   }
}

void sched_manager(void)
{
   int i, f;

   //printf("\nx_timer: %x",get_Time());
   for (i = 0, f = 0; i < SCHED_MAX; i++)
	  switch (sched_array[i].code)
	  {
		 case SCHED_CONTINUE:
			(sched_array[i].func)();
			f++;
			break;
		 case SCHED_PERIODIC:
			if (get_Time()>= sched_array[i].time)
			{
			   (sched_array[i].func)();
			   sched_array[i].time = get_Time()+ sched_array[i].period;
			}
			f++;
			break;
		 case SCHED_ONETIME:
			if (get_Time()>= sched_array[i].time)
			{
			   (sched_array[i].func)();
			   sched_delete(i);
			}
			f++;
			break;
		 case SCHED_NOTUSED:
			break;
		 default:
 /*                     print_allarm("SCHEDULER CASE", NOALARM);        */
		//	ERROR(0);
			sched_delete(i);
			break;
	  }
//   if (!f)
//	  ERROR(0);
/*        print_allarm("SCHEDULER EMPTY", NOALARM);     */
}


