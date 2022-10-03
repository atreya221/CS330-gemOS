#include<clone_threads.h>
#include<entry.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<mmap.h>

/*
  system call handler for clone, create thread like 
  execution contexts. Returns pid of the new context to the caller. 
  The new context starts execution from the 'th_func' and 
  use 'user_stack' for its stack
*/
long do_clone(void *th_func, void *user_stack, void *user_arg) 
{
	struct exec_context *new_ctx = get_new_ctx();
	struct exec_context *ctx = get_current_ctx();

	u32 pid = new_ctx->pid;
	
	if(!ctx->ctx_threads){  // This is the first thread
		ctx->ctx_threads = os_alloc(sizeof(struct ctx_thread_info));
		bzero((char *)ctx->ctx_threads, sizeof(struct ctx_thread_info));
		ctx->ctx_threads->pid = ctx->pid;
	}
		
	/* XXX Do not change anything above. Your implementation goes here*/
	
	
	// allocate page for os stack in kernel part of process's VAS
	// The following two lines should be there. The order can be 
	// decided depending on your logic.
	setup_child_context(new_ctx);
	*new_ctx = *ctx;
	new_ctx->pid = pid;
	new_ctx->type = EXEC_CTX_USER_TH;  // Make sure the context type is thread  
	
	new_ctx->regs.entry_rsp = (u64)user_stack;
	new_ctx->regs.entry_rip = (u64)th_func;
	new_ctx->regs.rdi = (u64)user_arg; 

	new_ctx->ppid = ctx->pid;
	new_ctx->state = WAITING;            // For the time being. Remove it as per your need.
	new_ctx->ctx_threads = NULL;
	
	
	int thread_id = -1;
	for(int i = 0; i < MAX_THREADS; i++)
	{
		if(ctx->ctx_threads->threads[i].status == TH_UNUSED) 
		{
			thread_id = i;
			break;
		}
	}
	if(thread_id == -1)return -1;


	ctx->ctx_threads->threads[thread_id].parent_ctx = ctx;
	ctx->ctx_threads->threads[thread_id].pid = new_ctx->pid;
	ctx->ctx_threads->threads[thread_id].status = TH_USED;

	for(int i = 0; i < MAX_PRIVATE_AREAS; i++)
	{
		ctx->ctx_threads->threads[thread_id].private_mappings[i].start_addr = -1;
		ctx->ctx_threads->threads[thread_id].private_mappings[i].length = 0;
		ctx->ctx_threads->threads[thread_id].private_mappings[i].owner = NULL;
		ctx->ctx_threads->threads[thread_id].private_mappings[i].flags = 0;
	}
	return new_ctx->pid;
}

/*This is the page fault handler for thread private memory area (allocated using 
 * gmalloc from user space). This should fix the fault as per the rules. If the the 
 * access is legal, the fault handler should fix it and return 1. Otherwise it should
 * invoke segfault_exit and return -1*/

int handle_thread_private_fault(struct exec_context *current, u64 addr, int error_code)
{
  
   /* your implementation goes here*/

   	u64 thread_owner = -1;
	u32 thread_flag = 0; 

	if(current->type == EXEC_CTX_USER_TH)
	{
		struct exec_context* parent = get_ctx_by_pid(current->ppid);
		int flag = 0;
		for(int i = 0; i < MAX_THREADS; i++)
		{
			for(int j = 0; j < MAX_PRIVATE_AREAS; j++)
			{
				if(addr >= parent->ctx_threads->threads[i].private_mappings[j].start_addr && addr <= parent->ctx_threads->threads[i].private_mappings[i].start_addr + parent->ctx_threads->threads[i].private_mappings[i].length)
				{
					thread_flag = parent->ctx_threads->threads[i].private_mappings[i].flags;
					thread_owner = parent->ctx_threads->threads[i].private_mappings[i].owner->pid;
					flag = 1;
					break;
				}
			}
			if(flag) break;
		}
	}

	if(current->type == EXEC_CTX_USER || thread_owner == current->pid)
	{	
		u64 pgd = current->pgd;

		u64 offset1 = (addr>>39) & 0x1ff;
		u64 offset2 = (addr>>30) & 0x1ff;
		u64 offset3 = (addr>>21) & 0x1ff;
		u64 offset4 = (addr>>12) & 0x1ff;
		
		u64* addr1 = (u64*) osmap(pgd) + offset1;
		if(!(u64*)*addr1) 
		{
			*addr1 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
			bzero((char*)*addr1, 4096);
			*addr1 = *addr1 | 0x7;
		}
		u64* addr2 = (u64*) (*addr1 & ~0xfff);
		addr2 += offset2;
		if(!(u64*)*addr2)
		{
			*addr2 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
			bzero((char*)*addr2, 4096);
			*addr2 = *addr2 | 0x7;
		}

		u64* addr3 = (u64*) (*addr2 & ~0xfff);
		addr3 += offset3;
		if(!(u64*)*addr3) 
		{
			*addr3 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
			bzero((char*)*addr3, 4096);
			*addr3 = *addr3 | 0x7;
		}

		u64* addr4 = (u64*) (*addr3 & ~0xfff);
		addr4 += offset4;
		if(!(u64*)*addr4) 
		{
			*addr4 = (u64) osmap(os_pfn_alloc(USER_REG));
			bzero((char*)*addr4, 4096);
			*addr4 = *addr4 | 0x7;
		}
		return 1;
	}
	else
	{
		if(error_code == 0x4 && thread_flag == 0x23)
		{
			u64 pgd = current->pgd;
			u64 offset1 = (addr>>39) & 0x1ff;
			u64 offset2 = (addr>>30) & 0x1ff;
			u64 offset3 = (addr>>21) & 0x1ff;
			u64 offset4 = (addr>>12) & 0x1ff;

			u64* addr1 = (u64*) osmap(pgd) + offset1;
			if(!(u64*)*addr1) 
			{
				*addr1 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
				bzero((char*)*addr1, 4096);
				*addr1 = *addr1 | 0x7;
			}

			u64* addr2 = (u64*) (*addr1 & ~0xfff);
			addr2 += offset2;
			if(!(u64*)*addr2) 
			{
				*addr2 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
				bzero((char*)*addr2, 4096);
				*addr2 = *addr2 | 0x7;
			}

			u64* addr3 = (u64*) (*addr2 & ~0xfff);
			addr3 += offset3;
			if(!(u64*)*addr3) 
			{
				*addr3 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
				bzero((char*)*addr3, 4096);
				*addr3 = *addr3 | 0x7;
			}

			u64* addr4 = (u64*) (*addr3 & ~0xfff);
			addr4 += offset4;
			if(!(u64*)*addr4) 
			{
				*addr4 = (u64) osmap(os_pfn_alloc(USER_REG));
				bzero((char*)*addr4, 4096);
				*addr4 = (*addr4 & ~0x7) | 0x5;
			}
			return 1;
		}
		else if((error_code == 0x4 && thread_flag == 0x43) || (error_code == 0x6 && thread_flag == 0x43))
		{
			u64 pgd = current->pgd;

			u64 offset1 = (addr>>39) & 0x1ff;
			u64 offset2 = (addr>>30) & 0x1ff;
			u64 offset3 = (addr>>21) & 0x1ff;
			u64 offset4 = (addr>>12) & 0x1ff;

			u64* addr1 = (u64*) osmap(pgd) + offset1;
			if(!(u64*)*addr1) 
			{
				*addr1 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
				bzero((char*)*addr1, 4096);
				*addr1 = *addr1 | 0x7;
			}

			u64* addr2 = (u64*) (*addr1 & ~0xfff);
			addr2 += offset2;
			if(!(u64*)*addr2) 
			{
				*addr2 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
				bzero((char*)*addr2, 4096);
				*addr2 = *addr2 | 0x7;
			}

			u64* addr3 = (u64*) (*addr2 & ~0xfff);
			addr3 += offset3;
			if(!(u64*)*addr3) 
			{
				*addr3 = (u64) osmap(os_pfn_alloc(OS_PT_REG));
				bzero((char*)*addr3, 4096);
				*addr3 = *addr3 | 0x7;
			}

			u64* addr4 = (u64*) (*addr3 & ~0xfff);
			addr4 += offset4;
			if(!(u64*)*addr4) 
			{
				*addr4 = (u64) osmap(os_pfn_alloc(USER_REG));
				bzero((char*)*addr4, 4096);
				*addr4 = *addr4 | 0x7;
			}
			return 1;
		}
	}
	segfault_exit(current->pid, current->regs.entry_rip, addr);
	return -1;
}

/*This is a handler called from scheduler. The 'current' refers to the outgoing context and the 'next' 
 * is the incoming context. Both of them can be either the parent process or one of the threads, but only
 * one of them can be the process (as we are having a system with a single user process). This handler
 * should apply the mapping rules passed in the gmalloc calls. */

int handle_private_ctxswitch(struct exec_context *current, struct exec_context *next)
{

	/* your implementation goes here*/

	if(!current || !next) return -1;

	struct exec_context* parent;
	if(current->type == EXEC_CTX_USER_TH)
	{
		parent = get_ctx_by_pid(current->ppid);
		if(!parent) return -1;
	}
	else parent = current;
	if(!parent->ctx_threads) return -1;

	if(current->type == EXEC_CTX_USER_TH && next->type == EXEC_CTX_USER)
	{
		struct thread* curr_th = find_thread_from_pid(parent, current->pid);
		if(curr_th == NULL)return -1;
		for(int i = 0; i < MAX_THREADS; i++)
		{
			if(curr_th->pid == parent->ctx_threads->threads[i].pid) continue;
			for(int j = 0; j < MAX_PRIVATE_AREAS; j++)
			{
				u64 start = parent->ctx_threads->threads[i].private_mappings[j].start_addr;
				u32 len = parent->ctx_threads->threads[i].private_mappings[j].length;
				if(start == -1 || len == 0) continue;

				for(u64 addr = start; addr <= start + len; addr += 4096)
				{
					u64 pgd = current->pgd;

					u64 offset1 = (addr>>39) & 0x1ff;
					u64 offset2 = (addr>>30) & 0x1ff;
					u64 offset3 = (addr>>21) & 0x1ff;
					u64 offset4 = (addr>>12) & 0x1ff;

					u64* addr1 = (u64*) osmap(pgd) + offset1;
					if(!(u64*)*addr1) break;
					*addr1 = *addr1 | 0x7;
					u64* addr2 = (u64*) (*addr1 & ~0xfff)+offset2;
					if(!(u64*)*addr2) break;
					*addr2 = *addr2 | 0x7;
					u64* addr3 = (u64*) (*addr2 & ~0xfff)+offset3;
					if(!(u64*)*addr3) break;
					*addr3 = *addr3 | 0x7;
					u64* addr4 = (u64*) (*addr3 & ~0xfff)+offset4;					
					if(!(u64*)*addr4) break;

					asm volatile ("invlpg (%0);" :: "r"(addr) 
									: "memory");
					
					*addr4 = *addr4 | 0x7;
				}
			}
		}	
		return 0;	
	}
	else if(current->type == EXEC_CTX_USER && next->type == EXEC_CTX_USER_TH)
	{
		struct thread* next_th = find_thread_from_pid(parent, next->pid);
		if(!next_th)return -1;

		for(int i = 0; i < MAX_THREADS; i++)
		{
			if(next_th->pid == parent->ctx_threads->threads[i].pid) continue;
			for(int j = 0; j < MAX_PRIVATE_AREAS; j++)
			{
				u64 start = parent->ctx_threads->threads[i].private_mappings[j].start_addr;
				u32 len = parent->ctx_threads->threads[i].private_mappings[j].length;
				if(start == -1 || len == 0) continue;

				for(u64 addr = start; addr <= start + len; addr += 4096)
				{
					u64 pgd = current->pgd;

					u64 offset1 = (addr>>39) & 0x1ff;
					u64 offset2 = (addr>>30) & 0x1ff;
					u64 offset3 = (addr>>21) & 0x1ff;
					u64 offset4 = (addr>>12) & 0x1ff;

					u64* addr1 = (u64*) osmap(pgd) + offset1;
					if(!(u64*)*addr1) break;
					*addr1 = *addr1 | 0x7;
					u64* addr2 = (u64*) (*addr1 & ~0xfff) + offset2;
					if(!(u64*)*addr2) break;
					*addr2 = *addr2 | 0x7;
					u64* addr3 = (u64*) (*addr2 & ~0xfff) + offset3;
					if(!(u64*)*addr3) break;
					*addr3 = *addr3 | 0x7;
					u64* addr4 = (u64*) (*addr3 & ~0xfff) + offset4;					
					if(!(u64*)*addr4) break;

					u32 flags = parent->ctx_threads->threads[i].private_mappings[j].flags;
					asm volatile ("invlpg (%0);" :: "r"(addr) 
									: "memory");
					if(flags == 0x13) *addr4 = (*addr4 & ~0x7) | 0x1;
					else if(flags == 0x23) *addr4 = (*addr4 & ~0x7) | 0x5;
					else if(flags == 0x43) *addr4 = (*addr4 & ~0x7) | 0x7;
					
				}
			}
		}
		return 0;
	}
	else if(current->type == EXEC_CTX_USER_TH && next->type == EXEC_CTX_USER_TH)
	{
		struct thread* curr_th = find_thread_from_pid(parent, current->pid);
		struct thread* next_th = find_thread_from_pid(parent, next->pid);
		if(!curr_th || !next_th) return -1;
		for(int i = 0; i < MAX_THREADS; i++)
		{
			if(next_th->pid != parent->ctx_threads->threads[i].pid && curr_th->pid != parent->ctx_threads->threads[i].pid) continue;
			for(int j = 0; j < MAX_PRIVATE_AREAS; j++)
			{
				u64 start = parent->ctx_threads->threads[i].private_mappings[j].start_addr;
				u32 len = parent->ctx_threads->threads[i].private_mappings[j].length;
				if(start == -1 || len == 0) continue;

				for(u64 addr = start; addr <= start + len; addr += 4096)
				{
					u64 pgd = current->pgd;

					u64 offset1 = (addr>>39) & 0x1ff;
					u64 offset2 = (addr>>30) & 0x1ff;
					u64 offset3 = (addr>>21) & 0x1ff;
					u64 offset4 = (addr>>12) & 0x1ff;

					u64* addr1 = (u64*) osmap(pgd) + offset1;
					if(!(u64*)*addr1) break;
					*addr1 = *addr1 | 0x7;
					u64* addr2 = (u64*) (*addr1 & ~0xfff) + offset2;
					if(!(u64*)*addr2) break;
					*addr2 = *addr2 | 0x7;
					u64* addr3 = (u64*) (*addr2 & ~0xfff) + offset3;
					if(!(u64*)*addr3) break;
					*addr3 = *addr3 | 0x7;
					u64* addr4 = (u64*) (*addr3 & ~0xfff) + offset4;					
					if(!(u64*)*addr4) break;

					asm volatile ("invlpg (%0)" :: "r"(addr) 
									: "memory");
					if(next_th->pid == parent->ctx_threads->threads[i].pid) *addr4 = *addr4 | 0x7;
					else if(curr_th->pid == parent->ctx_threads->threads[i].pid)
					{
						u32 flags = parent->ctx_threads->threads[i].private_mappings[j].flags;
						if(flags == 0x13) *addr4 = (*addr4 & ~0x7) | 0x1;
						else if(flags == 0x23) *addr4 = (*addr4 & ~0x7) | 0x5;
						else if(flags == 0x43) *addr4 = (*addr4 & ~0x7) | 0x7;
					}
				}
			}
		}
		return 0;	
	}
   	return -1;	
}