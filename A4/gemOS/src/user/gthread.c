#include <gthread.h>
#include <ulib.h>

static struct process_thread_info tinfo __attribute__((section(".user_data"))) = {};
/*XXX 
      Do not modifiy anything above this line. The global variable tinfo maintains user
      level accounting of threads. Refer gthread.h for definition of related structs.
 */


/* Returns 0 on success and -1 on failure */
/* Here you can use helper system call "make_thread_ready" for your implementation */


int gthread_exit_handler() {
	void* ret;
	asm volatile(
	"mov %%rax, %0;"
	: "=r" (ret) 
	:
	:"memory");
	gthread_exit(ret);
	return 0;
}

int gthread_create(int *tid, void *(*fc)(void *), void *arg) {
        
	/* You need to fill in your implementation here*/
	if(tinfo.num_threads == MAX_THREADS) return -1;

	void* stack_ptr = mmap(NULL, TH_STACK_SIZE, PROT_READ|PROT_WRITE, 0);
	if(!stack_ptr) return -1;

	*(u64*)((u64)stack_ptr + TH_STACK_SIZE - 8) = (u64)(&gthread_exit_handler);

	for(int i = 0; i < MAX_THREADS; i++)
	{
		if(tinfo.threads[i].status == TH_STATUS_UNUSED)
		{
			int thread_pid = clone(fc, ((u64)stack_ptr) + TH_STACK_SIZE - 8, arg);
			if(thread_pid < 0) return -1;		

			*tid = i;

			for (int idx = 0; idx < MAX_GALLOC_AREAS; idx++)
			{
				tinfo.threads[i].priv_areas[idx].start = -1;
				tinfo.threads[i].priv_areas[idx].length = 0;
				tinfo.threads[i].priv_areas[idx].owner = &(tinfo.threads[i]);
				tinfo.threads[i].priv_areas[idx].flags = 0;
			}

			tinfo.num_threads++;
			tinfo.threads[i].tid = i;
			tinfo.threads[i].pid = thread_pid;
			tinfo.threads[i].ret_addr = NULL;
			tinfo.threads[i].status = TH_STATUS_ALIVE;
			tinfo.threads[i].stack_addr = (void*)((u64)stack_ptr + TH_STACK_SIZE);

			make_thread_ready(thread_pid);
			return 0;
		}
	}
	return -1;
}

int gthread_exit(void *retval) {

	/* You need to fill in your implementation here*/
	int thread_pid = getpid();
	for(int i = 0; i < MAX_THREADS; i++)
	{
		if(tinfo.threads[i].status == TH_STATUS_ALIVE && tinfo.threads[i].pid == thread_pid)
		{
			tinfo.threads[i].status = TH_STATUS_USED;
			tinfo.threads[i].ret_addr = retval;
			break;
		}
	}
	//call exit
	exit(0);
}


void* gthread_join(int tid) {
        
	/* Here you can use helper system call "wait_for_thread" for your implementation */
	
	/* You need to fill in your implementation here*/

	if(tid < 0 && tid >= MAX_THREADS) return NULL;
	if(tinfo.threads[tid].status == TH_STATUS_UNUSED) return NULL;

	while(1)
	{  
		int val = wait_for_thread(tinfo.threads[tid].pid);
		if(val < 0) 
		{
			break;
		}
	}

	for(int i = 0; i < MAX_GALLOC_AREAS; i++)
	{
		if(tinfo.threads[tid].priv_areas[i].start == -1 || tinfo.threads[tid].priv_areas[i].length == 0) continue;
		if(munmap((u64*)tinfo.threads[tid].priv_areas[i].start, tinfo.threads[tid].priv_areas[i].length) < 0) return MAP_ERR;
		
		tinfo.threads[tid].priv_areas[i].start = -1;
		tinfo.threads[tid].priv_areas[i].length = 0;
		tinfo.threads[tid].priv_areas[i].owner = NULL;
		tinfo.threads[tid].priv_areas[i].flags = 0;
	}

	if(munmap(tinfo.threads[tid].stack_addr - TH_STACK_SIZE, TH_STACK_SIZE) < 0) return MAP_ERR;

	tinfo.threads[tid].status = TH_STATUS_UNUSED;
	tinfo.num_threads--;
	return tinfo.threads[tid].ret_addr;
}


/*Only threads will invoke this. No need to check if its a process
 * The allocation size is always < GALLOC_MAX and flags can be one
 * of the alloc flags (GALLOC_*) defined in gthread.h. Need to 
 * invoke mmap using the proper protection flags (for prot param to mmap)
 * and MAP_TH_PRIVATE as the flag param of mmap. The mmap call will be 
 * handled by handle_thread_private_map in the OS.
 * */

void* gmalloc(u32 size, u8 alloc_flag)
{
   
	/* You need to fill in your implementation here*/
	int thread_pid = getpid();

	for(int i = 0; i < MAX_THREADS; i++)	
	{
		if(tinfo.threads[i].status == TH_STATUS_ALIVE && tinfo.threads[i].pid == thread_pid)
		{			
			for(int j = 0; j < MAX_GALLOC_AREAS; j++)
			{
				if(!tinfo.threads[i].priv_areas[j].length)
				{
					void* addr_ptr = NULL;
					if(alloc_flag == GALLOC_OWNONLY)
					{
						addr_ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|TP_SIBLINGS_NOACCESS, MAP_TH_PRIVATE);
						if(!addr_ptr) return NULL;
					}
					else if(alloc_flag == GALLOC_OTRDONLY)
					{
						addr_ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|TP_SIBLINGS_RDONLY, MAP_TH_PRIVATE);
						if(!addr_ptr) return NULL;
					}
					else if(alloc_flag == GALLOC_OTRDWR)
					{
						addr_ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|TP_SIBLINGS_RDWR, MAP_TH_PRIVATE);
						if(!addr_ptr) return NULL;
					}
					else return NULL;

					tinfo.threads[i].priv_areas[j].start = (u64)addr_ptr;
					tinfo.threads[i].priv_areas[j].length = size;	
					tinfo.threads[i].priv_areas[j].owner = &(tinfo.threads[i]);
					tinfo.threads[i].priv_areas[j].flags = alloc_flag;

					return addr_ptr;
				}
			}		
			return NULL;
		}
	}

	return NULL;
}
/*
   Only threads will invoke this. No need to check if the caller is a process.
*/
int gfree(void *ptr)
{
   
    /* You need to fill in your implementation here*/
	int thread_pid = getpid();
	for(int i = 0; i < MAX_THREADS; i++)	
	{
		if(tinfo.threads[i].status == TH_STATUS_ALIVE && tinfo.threads[i].pid == thread_pid){
			for(int j = 0; j < MAX_GALLOC_AREAS; j++)
			{
				if((u64)ptr == tinfo.threads[i].priv_areas[j].start)
				{
					if(munmap(ptr, tinfo.threads[i].priv_areas[j].length) < 0) return -1;

					tinfo.threads[i].priv_areas[j].start = -1;
					tinfo.threads[i].priv_areas[j].length = 0;
					tinfo.threads[i].priv_areas[j].owner = NULL;
					tinfo.threads[i].priv_areas[j].flags = 0;
					return 0;
				}
			}
			return -1;
		}
	}
    return -1;
}