#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process info for the pipe.
struct pipe_info_per_process {

    // TODO:: Add members as per your need...
    int pid;
    int active_read;
    int active_write;
    int open_read;
    int open_write;

};

// Global information for the pipe.
struct pipe_info_global {

    char *pipe_buff;    // Pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...

    int pcnt;
    int read_ptr;
    int write_ptr;
    int buff_len;
    
};

// Pipe information structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct pipe_info {

    struct pipe_info_per_process pipe_per_proc [MAX_PIPE_PROC];
    struct pipe_info_global pipe_global;

};


// Function to allocate space for the pipe and initialize its members.
struct pipe_info* alloc_pipe_info () {
  
    // Allocate space for pipe structure and pipe buffer.
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign pipe buffer.
    pipe->pipe_global.pipe_buff = buffer;

    /*  TODO:: Initializing pipe fields
     *  
     *  Initialize per process fields for this pipe.
     *  Initialize global fields for this pipe.
     *
     */

    // Return the pipe.

    // initialise global ppipe vars
    pipe->pipe_global.pcnt = 1;
    pipe->pipe_global.read_ptr = 0;
    pipe->pipe_global.buff_len = 0;
    pipe->pipe_global.write_ptr = 0;

    // initialise per process vars for parent process (process_index = 0)
    pipe->pipe_per_proc[0].pid = get_current_ctx()->pid;
    pipe->pipe_per_proc[0].active_write = 1;
    pipe->pipe_per_proc[0].active_read = 1;
    pipe->pipe_per_proc[0].open_read = 1;
    pipe->pipe_per_proc[0].open_write = 1; 
    
    // initialise per process vars for other process
    for(int i = 1; i < MAX_PIPE_PROC; i++)
    {
        pipe->pipe_per_proc[i].pid = -1;
        pipe->pipe_per_proc[i].active_read = 0;
        pipe->pipe_per_proc[i].active_write = 0;
        pipe->pipe_per_proc[i].open_read = 0;
        pipe->pipe_per_proc[i].open_write = 0; 
    }
    return pipe;

}

// Function to free pipe buffer and pipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_pipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->pipe->pipe_global.pipe_buff);
    os_page_free(OS_DS_REG, filep->pipe);

}

// Fork handler for the pipe.
int do_pipe_fork (struct exec_context *child, struct file *filep) {

    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the pipe.
     *  This handler will be called twice since pipe has 2 file objects.
     *  Also consider the limit on no of processes a pipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */

    // error handling for null cases
    if(!filep || !child) return -EOTHERS;

    // find process index for new process    
    int process_index = -1;
    for(int i = 0; i < MAX_PIPE_PROC; i++)
    {
        if(filep->pipe->pipe_per_proc[i].pid != -1 && filep->pipe->pipe_per_proc[i].pid != child->pid) continue;
        if(filep->mode == O_WRITE) 
        {
            filep->pipe->pipe_per_proc[i].active_write = 1;
            process_index = i;
            break;
        }
        if(filep->mode == O_READ) 
        {
            filep->pipe->pipe_per_proc[i].active_read = 1;
            process_index = i;
            break;
        }
    }
    
    if(process_index == -1) return -EOTHERS;
    if(filep->pipe->pipe_per_proc[process_index].pid == -1) filep->pipe->pipe_global.pcnt++; // increment only first time (before pid is set)
    filep->pipe->pipe_per_proc[process_index].pid = child->pid;

    int parent_process_index = -1;
    struct exec_context* current = get_current_ctx();

    for(int i = 0; i < MAX_PIPE_PROC; i++)
    {
        if(filep->pipe->pipe_per_proc[i].pid == current->pid)
        {
            parent_process_index = i;
            break;
        }
    }
    if(parent_process_index == -1) return -EOTHERS;
    
    if(filep->mode == O_READ) filep->pipe->pipe_per_proc[process_index].open_read = filep->pipe->pipe_per_proc[parent_process_index].open_read;
    if(filep->mode == O_WRITE) filep->pipe->pipe_per_proc[process_index].open_write = filep->pipe->pipe_per_proc[parent_process_index].open_write;
    
    // Return successfully.

    return 0;

}

// Function to close the pipe ends and free the pipe when necessary.
long pipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the pipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the pipe.
     *  Use free_pipe() function to free pipe buffer and pipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *
     */

    int ret_value = -EOTHERS;

    if (filep == NULL) return ret_value;

    int process_index = -1;
    int pid = get_current_ctx()->pid;
    for(int i = 0; i < MAX_PIPE_PROC; i++)
    {
        if(pid == filep->pipe->pipe_per_proc[i].pid)
        {
            process_index = i;
            break;
        }
    }

    if (process_index == -1) return ret_value;

    if(filep->mode == O_READ) 
    {
        filep->pipe->pipe_per_proc[process_index].active_read = 0;
        filep->pipe->pipe_per_proc[process_index].open_read = 0;
    }

    if(filep->mode == O_WRITE) 
    {
        filep->pipe->pipe_per_proc[process_index].active_write = 0;
        filep->pipe->pipe_per_proc[process_index].open_write = 0;
    }

    // read and write ends of all processes closed
    if(!filep->pipe->pipe_per_proc[process_index].open_read && !filep->pipe->pipe_per_proc[process_index].open_write)
    {
        filep->pipe->pipe_global.pcnt -= 1;
        filep->pipe->pipe_per_proc[process_index].pid = 0;
    }

    if(!filep->pipe->pipe_global.pcnt) free_pipe(filep);
    
    // Close the file and return.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Check whether passed buffer is valid memory location for read or write.
int is_valid_mem_range (unsigned long buff, u32 count, int access_bit) {

    /**
     *  TODO:: Implementation for buffer memory range checking
     *
     *  Check whether passed memory range is suitable for read or write.
     *  If access_bit == 1, then it is asking to check read permission.
     *  If access_bit == 2, then it is asking to check write permission.
     *  If range is valid then return 1.
     *  Incase range is not valid or have some permission issue return -EBADMEM.
     *
     */

    int ret_value = -EBADMEM;
    struct exec_context* current = get_current_ctx();

    for(int i = 0; i < MAX_MM_SEGS - 1; i++)
    {
        if(buff >= current->mms[i].start && buff+count <= current->mms[i].next_free)
        {
            if((access_bit == 1 && current->mms[i].access_flags%2) || (access_bit == 2 && (current->mms[i].access_flags >> 1)%2)) return 1;
        }
    }

    if(buff >= current->mms[MAX_MM_SEGS-1].start && buff+count <= current->mms[MAX_MM_SEGS-1].end)
    {
        if((access_bit == 1 && current->mms[MAX_MM_SEGS-1].access_flags%2) || (access_bit == 2 && (current->mms[MAX_MM_SEGS-1].access_flags >> 1)%2)) return 1;
    } 

    struct vm_area *curr_area = current->vm_area;
    while(curr_area != NULL)
    {
        if(buff >= curr_area->vm_start && buff + count <= curr_area->vm_end)
        {
            if(access_bit == 1)
            {
                if(curr_area->access_flags%2) return 1;
            }
            else if(access_bit == 2)
            {
                if((curr_area->access_flags >> 1)%2) return 1;
            }
        }
        curr_area = curr_area->vm_next;
    }
    // Return the finding.
    return ret_value;

}

// Function to read given no of bytes from the pipe.
int pipe_read (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Read
     *
     *  Read the data from pipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the pipe then just read
     *       that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If read end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */
    
    
    int ret_fd = -EOTHERS;
    if (!filep || count < 0 || !buff) return ret_fd;

    ret_fd = -EACCES;
    if(filep->mode == O_WRITE) return ret_fd;

    if(is_valid_mem_range((unsigned long)buff,count,2) != 1) return ret_fd;

    int process_index = -1;
    int pid = get_current_ctx()->pid;
    for(int i = 0; i < MAX_PIPE_PROC; i++)
    {
        if(filep->pipe->pipe_per_proc[i].pid == pid) 
        {
            process_index = i;
            break;
        }
    }
    if (process_index == -1) return -EOTHERS;

    ret_fd = -EINVAL; 
    if(filep->pipe->pipe_per_proc[process_index].open_read)
    {
        int start_pos = filep->pipe->pipe_global.read_ptr;
        int len = (count > filep->pipe->pipe_global.buff_len) ? filep->pipe->pipe_global.buff_len : count;
        if(len <= 0) return 0;

        for(int i = 0;i < len; i++)
        {
            buff[i]=filep->pipe->pipe_global.pipe_buff[(i+start_pos)%MAX_PIPE_SIZE];
        }
        filep->pipe->pipe_global.read_ptr = (start_pos+len)%MAX_PIPE_SIZE;
        filep->pipe->pipe_global.buff_len -= len;
        return len;
    }
    return ret_fd;

}

int pipe_write (struct file *filep, char *buff, u32 count) {

    /*  TODO:: Implementation of Pipe Write
     *
     *  Write the data from the provided buffer to the pipe buffer.
     *  If count is greater than available space in the pipe then just write data
     *       that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If write end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */
    
    int ret_fd = -EOTHERS;
    if (!filep || count < 0 || !buff) return ret_fd;

    ret_fd = -EACCES;
    if(filep->mode == O_READ) return ret_fd;

    if(is_valid_mem_range((unsigned long)buff,count,1) != 1) return ret_fd;

    int process_index = -1;
    int pid = get_current_ctx()->pid;
    for(int i = 0; i < MAX_PIPE_PROC; i++)
    {
        if(filep->pipe->pipe_per_proc[i].pid == pid) 
        {
            process_index = i;
            break;
        }
    }
    if (process_index == -1) return -EOTHERS;

    ret_fd = -EINVAL; 
    if(filep->pipe->pipe_per_proc[process_index].open_write)
    {
        int start_pos = filep->pipe->pipe_global.write_ptr;
        count = (count >= MAX_PIPE_SIZE - filep->pipe->pipe_global.buff_len)? MAX_PIPE_SIZE - filep->pipe->pipe_global.buff_len: count;

        for(int i = 0; i < count; i++)
        {
            filep->pipe->pipe_global.pipe_buff[(i+start_pos)%MAX_PIPE_SIZE]=buff[i];
        }
        
        filep->pipe->pipe_global.buff_len += count;
        filep->pipe->pipe_global.write_ptr=(count+start_pos)%MAX_PIPE_SIZE;
        return count;
    }
    return ret_fd;
}

// Function to create pipe.
int create_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of Pipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function. 
     *  Create pipe_info object by invoking the alloc_pipe_info() function and
     *  fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *       -ENOMEM: If memory is not enough.
     *       -EOTHERS: Some other errors.
     *
     */

    // Simple return.

    int ret_val = -EOTHERS;
    if (!current || !fd) return ret_val;

    struct file* file1 = alloc_file();
    struct file* file2 = alloc_file();
    struct pipe_info* pipe1 = alloc_pipe_info();
    
    file1->pipe = pipe1;
    file2->pipe = pipe1;

    file1->offp = 0;
    file2->offp = 0;

    file1->ref_count = 1;
    file2->ref_count = 1;

    file1->inode = NULL;
    file2->inode = NULL;

    file1->type = PIPE;
    file2->type = PIPE;

    file1->mode = O_READ;
    file2->mode = O_WRITE;

    file1->fops->read = pipe_read;
    file2->fops->write = pipe_write;

    file1->fops->close = pipe_close;
    file2->fops->close = pipe_close;

    // find free file descriptors starting from 0
    fd[0] = 0;
    while(current->files[fd[0]])
    {
        fd[0]++;
        if(fd[0] > MAX_OPEN_FILES) return -ENOMEM;
    }

    fd[1] = fd[0];
    current->files[fd[0]] = file1;
    
    while(current->files[fd[1]])
    {
        fd[1]++;
        if(fd[1]>MAX_OPEN_FILES) return -ENOMEM;
    }

    current->files[fd[1]]=file2;
    
    return 0;

}