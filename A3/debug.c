#include <context.h>
#include <debug.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/*
 * allocate the struct which contains information about debugger
 *
 */
struct debug_info* alloc_debug_info() 
{
    struct debug_info* info = (struct debug_info*) os_alloc(sizeof(struct debug_info));
    if (info)
        bzero((char*) info, sizeof(struct debug_info));
    return info;
}
/*
 * frees a debug_info struct
 */
void free_debug_info(struct debug_info* ptr) 
{
    if (ptr)
        os_free((void*) ptr, sizeof(struct debug_info));
}



/*
 * allocates a page to store registers structure
 */
struct registers* alloc_regs() 
{
    struct registers* info = (struct registers*) os_alloc(sizeof(struct registers));
    if (info)
        bzero((char*) info, sizeof(struct registers));
    return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers* ptr) 
{
    if (ptr)
        os_free((void*) ptr, sizeof(struct registers));
}

/*
 * allocate a node for breakpoint list
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info* ptr) 
{
    if (ptr)
        os_free((void*) ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	// //printk("DEBUGGER FORK HANDLER CALLED\n");
	child_ctx->dbg = NULL;
	child_ctx->state = WAITING;
}


/******************************************************************************/

int function_on_stack(struct exec_context* ctx, void* addr) 
{  
    struct call_stack* curr_node = ctx->dbg->callstack;
    for(int i = 0; i < MAX_BACKTRACE; i++)
    {
        if((u64) addr == curr_node[i].brkpt_addr) return 1;
    }
    return 0;
}


struct call_stack* alloc_callstack() 
{
    struct call_stack* info = (struct call_stack*) os_alloc((MAX_BACKTRACE+1) * sizeof(struct call_stack));
    return info;
}

void free_callstack(struct call_stack* ptr)
{
    if (ptr) os_free((void*) ptr, (MAX_BACKTRACE+1) * sizeof(struct call_stack));
}


void reg_snapshot(struct registers* regptr1, struct user_regs regptr2) 
{
    regptr1->r15 = regptr2.r15;
    regptr1->r14 = regptr2.r14;
    regptr1->r13 = regptr2.r13;
    regptr1->r12 = regptr2.r12;
    regptr1->r11 = regptr2.r11;
    regptr1->r10 = regptr2.r10;
    regptr1->r9 = regptr2.r9;
    regptr1->r8 = regptr2.r8;
    regptr1->rbp = regptr2.rbp;
    regptr1->rdi = regptr2.rdi;
    regptr1->rsi = regptr2.rsi;
    regptr1->rdx = regptr2.rdx;
    regptr1->rcx = regptr2.rcx;
    regptr1->rbx = regptr2.rbx;
    regptr1->rax = regptr2.rax;
    regptr1->entry_rip = regptr2.entry_rip-1;
    regptr1->entry_cs = regptr2.entry_cs;
    regptr1->entry_rflags = regptr2.entry_rflags;
    regptr1->entry_rsp = regptr2.entry_rsp;
    regptr1->entry_ss = regptr2.entry_ss;
}

/* This is the int 0x3 handler
 * Hit from the childs context
 */

long int3_handler(struct exec_context* ctx) {
	if(!ctx || ctx->dbg) return -1;
	
	u32 ppid = ctx->ppid;
	struct exec_context* parent_ctx = get_ctx_by_pid(ppid);
	if(!parent_ctx) return -1;

	u64 brkpt_addr = (ctx->regs.entry_rip) - 1;
	struct breakpoint_info* head = parent_ctx->dbg->head;
	int temp = 0;
	while(head)
    {
		if(brkpt_addr == head->addr)
        {
			temp = 1;
			break;
		}
		head = head->next;
	}

	if(temp)
    {
        reg_snapshot(parent_ctx->dbg->childregs, ctx->regs);
		struct breakpoint_info* curr_node = parent_ctx->dbg->head;
        int flag = 0;

		while (curr_node) 
        {
			if (brkpt_addr ==curr_node->addr) flag=curr_node->end_breakpoint_enable;
			curr_node = curr_node->next;
		}
		
		if(flag)
        {
			for(int i = 0; i < MAX_BACKTRACE; i++)
            {
                if(!parent_ctx->dbg->callstack[i].brkpt_addr)
                {
                    parent_ctx->dbg->callstack[i].brkpt_addr=brkpt_addr;
                    parent_ctx->dbg->callstack[i].ret_addr=*((u64*) ctx->regs.entry_rsp);
                    break;
                }
            }
            *((u64*) ctx->regs.entry_rsp) = (u64) parent_ctx->dbg->end_handler;
		}
		
		ctx->regs.entry_rsp = ctx->regs.entry_rsp - sizeof(ctx->regs.rbp);
        *((u64*) ctx->regs.entry_rsp) = ctx->regs.rbp;

        u64 rbp = ctx->regs.entry_rsp;
        u64* rptr = (u64*) (rbp + 0x8);

		int index = 0;
		for (int i = 0; i < MAX_BACKTRACE; i++)
        {
			if(!parent_ctx->dbg->callstack[i].brkpt_addr)
            {
				index = i;
				break;
			}
		}

        int backtrace_index = 0;
        if ((u64) parent_ctx->dbg->end_handler != brkpt_addr) 
        {
            parent_ctx->dbg->backtrace[backtrace_index++] = brkpt_addr;
        }

		if ((u64) parent_ctx->dbg->end_handler == *rptr) 
        {
            parent_ctx->dbg->backtrace[backtrace_index++] = parent_ctx->dbg->callstack[index-1].ret_addr;
			index--;
        }
		else parent_ctx->dbg->backtrace[backtrace_index++] = *rptr;

        while (*rptr != END_ADDR && backtrace_index < MAX_BACKTRACE)
        {
            rbp = *((u64*) rbp);
            rptr = (u64*) (rbp + 0x8);
			if(*rptr == END_ADDR) break;
            if (*rptr == (u64) parent_ctx->dbg->end_handler) 
            {
            	parent_ctx->dbg->backtrace[backtrace_index++] = parent_ctx->dbg->callstack[index-1].ret_addr;
				index--;
        	}
            else parent_ctx->dbg->backtrace[backtrace_index++] = *rptr;
        }
        
        parent_ctx->dbg->backtrace[backtrace_index] = END_ADDR;
        parent_ctx->regs.rax = brkpt_addr;
		parent_ctx->state = READY;
        ctx->state = WAITING;
        schedule(parent_ctx);
	}
	else
    {
		int index = MAX_BACKTRACE;
		for(int i = 0; i < MAX_BACKTRACE;i++){
			if(!parent_ctx->dbg->callstack[i].brkpt_addr)
            {
				index=i;
				break;
			}
		}

		ctx->regs.entry_rsp = ctx->regs.entry_rsp - sizeof(parent_ctx->dbg->callstack[index-1].ret_addr);
        *((u64*) ctx->regs.entry_rsp) = parent_ctx->dbg->callstack[index-1].ret_addr;

        reg_snapshot(parent_ctx->dbg->childregs, ctx->regs);
		ctx->regs.entry_rsp = ctx->regs.entry_rsp - sizeof(ctx->regs.rbp);
        *((u64*) ctx->regs.entry_rsp) = ctx->regs.rbp;

        u64 rbp = ctx->regs.entry_rsp;
        u64* rptr = (u64*) (rbp + 0x8);
        int backtrace_index = 0;
        parent_ctx->dbg->backtrace[backtrace_index] = *rptr;
		backtrace_index++;
        index--;
        while (backtrace_index < MAX_BACKTRACE && *rptr != END_ADDR) 
        {
            rbp = *((u64*) rbp);
            rptr = (u64*) (rbp + 0x8);
			if(*rptr==END_ADDR) break;
            if (*rptr == (u64) parent_ctx->dbg->end_handler) 
            {
            	parent_ctx->dbg->backtrace[backtrace_index++] = parent_ctx->dbg->callstack[index-1].ret_addr;
				index--;
        	}
            else parent_ctx->dbg->backtrace[backtrace_index++] = *rptr;
        }
        parent_ctx->dbg->backtrace[backtrace_index] = END_ADDR;

		index = MAX_BACKTRACE;
		for(int i = 0; i < MAX_BACKTRACE; i++)
        {
			if(!parent_ctx->dbg->callstack[i].brkpt_addr)
            {
				index = i;
				break;
			}
		}

        parent_ctx->regs.rax = brkpt_addr;
        parent_ctx->dbg->callstack[index-1].brkpt_addr = 0;
		parent_ctx->dbg->callstack[index-1].ret_addr = 0;

		parent_ctx->state = READY;
        ctx->state = WAITING;

        schedule(parent_ctx);
	}

	return 0;
}

/*
 * Exit handler.
 * Deallocate the debug_info struct if its a debugger.
 * Wake up the debugger if its a child
 */
void debugger_on_exit(struct exec_context* ctx) {
    if (!ctx->dbg)
    {
        struct exec_context* parent = get_ctx_by_pid(ctx->ppid);
        if (parent->dbg && parent->dbg->child_pid == ctx->pid)
        {
            parent->regs.rax = CHILD_EXIT;
            parent->state = READY;
        }
    }
    else
    {
        struct breakpoint_info* curr_node = ctx->dbg->head;
        while (curr_node) 
        {
            struct breakpoint_info* node = curr_node;
            curr_node = curr_node->next;
            free_breakpoint_info(node);
        }
        free_callstack(ctx->dbg->callstack);
        free_regs(ctx->dbg->childregs);
        os_free(ctx->dbg->backtrace, (MAX_BACKTRACE+1) * sizeof(u64));
        free_debug_info(ctx->dbg);
    }
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context* ctx, void* addr) {
    if (!addr) return -1;
    if (!ctx) return -1;

    struct debug_info* debug_struct = alloc_debug_info();
    if (!debug_struct) return -1;

    debug_struct->breakpoint_count = 0;
    debug_struct->breakpoint_index = 0;
    debug_struct->head = NULL;
    debug_struct->end_handler = addr;
    debug_struct->child_pid_flag = 0;
    debug_struct->child_pid = -1;

    u64* backtrace = (u64*) os_alloc((MAX_BACKTRACE+1) * sizeof(u64));
    if(!backtrace) return -1;
    debug_struct->backtrace = backtrace;

    struct registers* childregs = alloc_regs();
    if (!childregs) return -1;
    debug_struct->childregs = childregs;

    struct call_stack* callstack = alloc_callstack();
    if(!callstack) return -1;
    debug_struct->callstack = callstack;

    for(int i = 0; i <= MAX_BACKTRACE; i++)
    {
        debug_struct->callstack[i].brkpt_addr = 0;
        debug_struct->callstack[i].ret_addr = 0;
    }
    
    ctx->dbg = debug_struct;
    *((u32*) addr) = (*((u32*) addr) & ~0xff) | INT3_OPCODE;
    return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context* ctx, void* addr, int flag) {
    if(!addr) return -1;
    if (!ctx || !ctx->dbg) return -1;
    
    struct breakpoint_info* head = ctx->dbg->head;
    if (!head) 
    {
        if (function_on_stack(ctx, addr)) return -1;
        
        struct breakpoint_info* node = alloc_breakpoint_info();
        if (!node) return -1;

        ctx->dbg->breakpoint_count++;
        ctx->dbg->breakpoint_index++;

        node->end_breakpoint_enable = flag;
        node->num = ctx->dbg->breakpoint_index;
        node->addr = (u64) addr;
        node->next = NULL;

        ctx->dbg->head = node;
        *((u32*) addr) = (*((u32*) addr) & ~0xff) | INT3_OPCODE;
        return 0;
    } 
    else 
    {
        struct breakpoint_info* curr_node = head;
        struct breakpoint_info* tail_node;
        while (curr_node) 
        {
            if ((u64) addr == curr_node->addr) 
            {
                if (function_on_stack(ctx, addr) && curr_node->end_breakpoint_enable && !flag) return -1;
                curr_node->end_breakpoint_enable = flag;
                return 0;
            }
            if (!curr_node->next) tail_node = curr_node;
            curr_node = curr_node->next;
        }

        if (ctx->dbg->breakpoint_count == MAX_BREAKPOINTS) return -1;
        struct breakpoint_info* node = alloc_breakpoint_info();
        if (!node) return -1;

        ctx->dbg->breakpoint_count++;
        ctx->dbg->breakpoint_index++;

        node->end_breakpoint_enable = flag;
        node->num = ctx->dbg->breakpoint_index;
        node->addr = (u64) addr;
        node->next = NULL;
        tail_node->next = node;

        *((u32*) addr) = (*((u32*) addr) & ~0xff) | INT3_OPCODE;
        return 0;
    }
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context* ctx, void* addr) {
    if(!addr) return -1;
    if (!ctx || !ctx->dbg) return -1;

    struct breakpoint_info* head = ctx->dbg->head;
    if (!head) return -1;

    if ((u64) addr == head->addr) 
    {
        if (head->end_breakpoint_enable && function_on_stack(ctx, addr)) return -1;

        ctx->dbg->breakpoint_count--;
        ctx->dbg->head = head->next;
        *((u32*) addr) = (*((u32*) addr) & ~0xff) | PUSHRBP_OPCODE;
        free_breakpoint_info(head);
        return 0;
    }

    struct breakpoint_info* prev_node = head;
    struct breakpoint_info* curr_node = head->next;
    while (curr_node) 
    {
        if ((u64) addr == curr_node->addr)
        {
            if (curr_node->end_breakpoint_enable && function_on_stack(ctx, addr)) return -1;
            
            prev_node->next = curr_node->next;
            ctx->dbg->breakpoint_count--;
            *((u32*) addr) = (*((u32*) addr) & ~0xff) | PUSHRBP_OPCODE;
            free_breakpoint_info(curr_node);
            return 0;
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
    return -1;
}

/*
 * called from debuggers context
 */

int do_info_breakpoints(struct exec_context* ctx, struct breakpoint* ubp) {
    if(!ubp) return -1;
    if (!ctx || !ctx->dbg) return -1;

    struct breakpoint_info* curr_node = ctx->dbg->head;
    int index = 0;
    while (curr_node) 
    {
        ubp[index].end_breakpoint_enable = curr_node->end_breakpoint_enable;
        ubp[index].addr = curr_node->addr;
        ubp[index].num = curr_node->num;
        curr_node = curr_node->next;
        index++;
    }

    if (index != ctx->dbg->breakpoint_count) return -1;
    return index;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context* ctx, struct registers* regs) {
    if (!regs) return -1;
    if (!ctx || !ctx->dbg) return -1;

    regs->r15 = ctx->dbg->childregs->r15;
    regs->r14 = ctx->dbg->childregs->r14;
    regs->r13 = ctx->dbg->childregs->r13;
    regs->r12 = ctx->dbg->childregs->r12;
    regs->r11 = ctx->dbg->childregs->r11;
    regs->r10 = ctx->dbg->childregs->r10;
    regs->r9 = ctx->dbg->childregs->r9;
    regs->r8 = ctx->dbg->childregs->r8;

    regs->rbp = ctx->dbg->childregs->rbp;
    regs->rdi = ctx->dbg->childregs->rdi;
    regs->rsi = ctx->dbg->childregs->rsi;

    regs->rdx = ctx->dbg->childregs->rdx;
    regs->rcx = ctx->dbg->childregs->rcx;
    regs->rbx = ctx->dbg->childregs->rbx;
    regs->rax = ctx->dbg->childregs->rax;

    regs->entry_rip = ctx->dbg->childregs->entry_rip;
    regs->entry_cs = ctx->dbg->childregs->entry_cs;
    regs->entry_rflags = ctx->dbg->childregs->entry_rflags;
    regs->entry_rsp = ctx->dbg->childregs->entry_rsp;
    regs->entry_ss = ctx->dbg->childregs->entry_ss;

    return 0;
}

/*
 * Called from debuggers context
 */
int do_backtrace(struct exec_context* ctx, u64 bt_buf) {
    int index = 0;
    if (!ctx || !ctx->dbg) return -1;
    if(!((u64*) bt_buf)) return -1;

    while (1) 
    {
        if(ctx->dbg->backtrace[index] == END_ADDR) break;
        ((u64*) bt_buf)[index] = ctx->dbg->backtrace[index];
        index++;
    }
    return index;
}

/*
 * When the debugger calls wait
 * it must move to WAITING state
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context* ctx) {
    if (!ctx || !ctx->dbg) return -1;
    if (!ctx->dbg->child_pid_flag)
    {
        for (int i = 1; i < MAX_PROCESSES; i++) 
        {
            struct exec_context* child_pcb = get_ctx_by_pid(i);
            if (child_pcb && child_pcb->state != UNUSED && child_pcb->ppid == ctx->pid) 
            {
                ctx->dbg->child_pid_flag = 1;
                ctx->dbg->child_pid = child_pcb->pid;
                break;
            }
        }
    } 
    else if (ctx->dbg->child_pid == -1) return -1;

    struct exec_context* child_pcb = get_ctx_by_pid(ctx->dbg->child_pid);
    if (!child_pcb) return -1;
    if (child_pcb->state == UNUSED) return CHILD_EXIT;

    ctx->state = WAITING;
    child_pcb->state = READY;
    schedule(child_pcb);
}