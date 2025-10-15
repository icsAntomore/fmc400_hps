typedef struct {
      short code;
	  void (*func)(void);
	  unsigned int time;
	  short period;
	  }  sched_slot;

#define SCHED_MAX 15                /* ex 30 */
#define SCHED_NOTUSED 0
#define SCHED_CONTINUE 1
#define SCHED_PERIODIC 2
#define SCHED_ONETIME 3
		

int sched_insert(int code, void (*func)(void), unsigned int x_timer);
int sched_del_by_func(void (*func)(void));
void sched_rep_by_func(int code, void (*func)(void), unsigned int x_timer);
void sched_rep_time_by_func(void (*func)(void), unsigned int x_timer);
void sched_manager(void);
int sched_find_func(void (*func)(void));
int sched_delete(int slot);
int sched_del_all_func(void);
unsigned long get_Time(void);
