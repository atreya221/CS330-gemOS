#include<ppipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process information for the ppipe.
struct ppipe_info_per_process {

    // TODO:: Add members as per your need...
    int pid;
    int active_read;
    int active_write;
    int open_read;
    int open_write;
    int read_ptr;
    int buff_len;

};

// Global information for the ppipe.
struct ppipe_info_global {

    char *ppipe_buff;       // Persistent pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...
    int pcnt;
    int write_ptr;
    int global_aval_space;

};

// Persistent pipe structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct ppipe_info {

    struct ppipe_info_per_process ppipe_per_proc [MAX_PPIPE_PROC];
    struct ppipe_info_global ppipe_global;

};


// Function to allocate space for the ppipe and initialize its members.
struct ppipe_info* alloc_ppipe_info() {

    // Allocate space for ppipe structure and ppipe buffer.
    struct ppipe_info *ppipe = (struct ppipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign ppipe buffer.
    ppipe->ppipe_global.ppipe_buff = buffer;

    /**
     *  TODO:: Initializing pipe fields
     *
     *  Initialize per process fields for this ppipe.
     *  Initialize global fields for this ppipe.
     *
     */ 

    // Return the ppipe.

    // initialise global ppipe vars
    ppipe->ppipe_global.pcnt = 1;
    ppipe->ppipe_global.global_aval_space = MAX_PPIPE_SIZE;
    ppipe->ppipe_global.write_ptr = 0;

    // initialise per process vars for parent process (process_index = 0)
    ppipe->ppipe_per_proc[0].pid = get_current_ctx()->pid;
    ppipe->ppipe_per_proc[0].active_read = 1;
    ppipe->ppipe_per_proc[0].active_write = 1;
    ppipe->ppipe_per_proc[0].open_read = 1;
    ppipe->ppipe_per_proc[0].open_write = 1; 
    ppipe->ppipe_per_proc[0].read_ptr = 0;
    ppipe->ppipe_per_proc[0].buff_len = 0;
    
    // initialise per process vars for other process
    for(int i = 1; i < MAX_PPIPE_PROC; i++)
    {
        ppipe->ppipe_per_proc[i].pid = -1;
        ppipe->ppipe_per_proc[i].active_write = 0;
        ppipe->ppipe_per_proc[i].active_read = 0;
        ppipe->ppipe_per_proc[i].open_read = 0;
        ppipe->ppipe_per_proc[i].read_ptr = 0;
        ppipe->ppipe_per_proc[i].buff_len = 0;
        ppipe->ppipe_per_proc[i].open_write = 0; 
    }
    return ppipe;

}

// Function to free ppipe buffer and ppipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_ppipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->ppipe->ppipe_global.ppipe_buff);
    os_page_free(OS_DS_REG, filep->ppipe);

} 

// Fork handler for ppipe.
int do_ppipe_fork (struct exec_context *child, struct file *filep) {
    
    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the ppipe.
     *  This handler will be called twice since ppipe has 2 file objects.
     *  Also consider the limit on no of processes a ppipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */

    // error handling for null cases
    if(!filep || !child) return -EOTHERS;

    // find process index for new process
    int process_index = -1;
    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        if(filep->ppipe->ppipe_per_proc[i].pid != -1 && filep->ppipe->ppipe_per_proc[i].pid != child->pid) continue;
        if(filep->mode == O_WRITE) 
        {
            filep->ppipe->ppipe_per_proc[i].active_write = 1;
            process_index = i;
            break;
        }
        if(filep->mode == O_READ) 
        {
            filep->ppipe->ppipe_per_proc[i].active_read = 1;
            process_index = i;
            break;
        }
    }
    if(process_index == -1) return -EOTHERS;
    if(filep->ppipe->ppipe_per_proc[process_index].pid == -1) filep->ppipe->ppipe_global.pcnt++; // increment only first time (before pid is set)
    filep->ppipe->ppipe_per_proc[process_index].pid = child->pid;

    int parent_process_index = -1;
    struct exec_context* current = get_current_ctx();

    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        if(filep->ppipe->ppipe_per_proc[i].pid == current->pid)
        {
            parent_process_index = i;
            break;
        }
    }
    if(parent_process_index == -1) return -EOTHERS;

    if(filep->mode == O_READ) filep->ppipe->ppipe_per_proc[process_index].open_read = filep->ppipe->ppipe_per_proc[parent_process_index].open_read;
    if(filep->mode == O_WRITE) filep->ppipe->ppipe_per_proc[process_index].open_write = filep->ppipe->ppipe_per_proc[parent_process_index].open_write;
    filep->ppipe->ppipe_per_proc[process_index].read_ptr = filep->ppipe->ppipe_per_proc[parent_process_index].read_ptr;
    filep->ppipe->ppipe_per_proc[process_index].buff_len = filep->ppipe->ppipe_per_proc[parent_process_index].buff_len;
    
    // Return successfully.

    return 0;

}


// Function to close the ppipe ends and free the ppipe when necessary.
long ppipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the ppipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the ppipe.
     *  Use free_pipe() function to free ppipe buffer and ppipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *                                                                          
     */
    int ret_value = -EOTHERS;

    if (!filep) return ret_value;

    int process_index = -1;
    int pid = get_current_ctx()->pid;
    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        if(pid == filep->ppipe->ppipe_per_proc[i].pid) 
        {
            process_index = i;
            break;
        }
    }

    if(process_index == -1) return ret_value;

    if(filep->mode == O_READ) 
    {
        filep->ppipe->ppipe_per_proc[process_index].active_read = 0;
        filep->ppipe->ppipe_per_proc[process_index].open_read = 0;
    }

    if(filep->mode == O_WRITE) 
    {
        filep->ppipe->ppipe_per_proc[process_index].active_write = 0;
        filep->ppipe->ppipe_per_proc[process_index].open_write = 0;   
    }

    // both read and write ends of the process closed
    if(filep->ppipe->ppipe_per_proc[process_index].open_read == 0 && filep->ppipe->ppipe_per_proc[process_index].open_write == 0)
    {
        filep->ppipe->ppipe_global.pcnt -= 1;
        filep->ppipe->ppipe_per_proc[process_index].pid = -1;
        filep->ppipe->ppipe_per_proc[process_index].read_ptr = 0;
        filep->ppipe->ppipe_per_proc[process_index].buff_len = 0;
    }

    // if only one process remaining with both ends closed
    if(!filep->ppipe->ppipe_global.pcnt) free_ppipe(filep);
    
    // Close the file and return.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Function to perform flush operation on ppipe.
int do_flush_ppipe (struct file *filep) {

    /**
     *  TODO:: Implementation of Flush system call
     *
     *  Reclaim the region of the persistent pipe which has been read by 
     *      all the processes.
     *  Return no of reclaimed bytes.
     *  In case of any error return -EOTHERS.
     *
     */

    if(!filep) return -EOTHERS;
    
    int reclaimed_bytes = 0;
    int max_buff_offset = -1;
    int min_read_ptr;
    int f = 0;

    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        if(filep->ppipe->ppipe_per_proc[i].active_read && filep->ppipe->ppipe_per_proc[i].buff_len > max_buff_offset)
        {
            f = 1;
            max_buff_offset = filep->ppipe->ppipe_per_proc[i].buff_len;
            min_read_ptr = filep->ppipe->ppipe_per_proc[i].read_ptr;
        }
    }
    if(!f) return 0;

    int prev_flush_pos = ((filep->ppipe->ppipe_global.write_ptr - MAX_PPIPE_SIZE + filep->ppipe->ppipe_global.global_aval_space)+MAX_PPIPE_SIZE)%MAX_PPIPE_SIZE;
    reclaimed_bytes = ((min_read_ptr - prev_flush_pos)+MAX_PPIPE_SIZE)%MAX_PPIPE_SIZE;
    if(max_buff_offset == 0 && reclaimed_bytes == 0 && filep->ppipe->ppipe_global.global_aval_space != MAX_PPIPE_SIZE) reclaimed_bytes = MAX_PPIPE_SIZE;

    filep->ppipe->ppipe_global.global_aval_space += reclaimed_bytes;
    return reclaimed_bytes;
}

// Read handler for the ppipe.
int ppipe_read (struct file *filep, char *buff, u32 count) {
    
    /**
     *  TODO:: Implementation of PPipe Read
     *
     *  Read the data from ppipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the ppipe then just read
     *      that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If read end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */

    int ret_fd = -EOTHERS;
    if (!filep || count < 0 || !buff) return ret_fd;

    ret_fd = -EACCES;
    if(filep->mode == O_WRITE) return ret_fd;
    
    ret_fd = -EOTHERS;
    int pid = get_current_ctx()->pid;
    int process_index = -1;

    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        if(filep->ppipe->ppipe_per_proc[i].pid == pid) 
        {
            process_index = i;
            break;
        }
    }
    if(process_index == -1) return ret_fd;

    ret_fd = -EINVAL; 
    //printk("from read in process %d\n", process_index);
    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        //printk("process : %d , active_read : %d, buff_offset : %d, read_ptr : %d\n", i, filep->ppipe->ppipe_per_proc[i].active_read, filep->ppipe->ppipe_per_proc[i].buff_len, filep->ppipe->ppipe_per_proc[i].read_ptr);
    }

    if(filep->ppipe->ppipe_per_proc[process_index].open_read)
    {
        int start_pos = filep->ppipe->ppipe_per_proc[process_index].read_ptr;
        int len = (count >= filep->ppipe->ppipe_per_proc[process_index].buff_len)?filep->ppipe->ppipe_per_proc[process_index].buff_len:count;
        if(len <= 0) return 0;

        for(int i = 0; i < len; i++)
        {
            buff[i]=filep->ppipe->ppipe_global.ppipe_buff[(i + start_pos) % MAX_PPIPE_SIZE];
        }
        filep->ppipe->ppipe_per_proc[process_index].read_ptr = (start_pos+len) % MAX_PPIPE_SIZE;
        filep->ppipe->ppipe_per_proc[process_index].buff_len -= len;
        return len;
    }
    return ret_fd;
}

// Write handler for ppipe.
int ppipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of PPipe Write
     *
     *  Write the data from the provided buffer to the ppipe buffer.
     *  If count is greater than available space in the ppipe then just write
     *      data that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If write end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */

    int ret_fd = -EOTHERS;
    if (!filep || count < 0 || !buff) return ret_fd;

    ret_fd = -EACCES;
    if(filep->mode == O_READ) return ret_fd;
    
    int process_index = -1;
    int pid = get_current_ctx()->pid;
    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        if(filep->ppipe->ppipe_per_proc[i].pid == pid) 
        {
            process_index = i;
            break;
        }
    }
    if (process_index == -1) return -EOTHERS;

    //printk("from write in process %d\n", process_index);
    for(int i = 0; i < MAX_PPIPE_PROC; i++)
    {
        //printk("process : %d , active_read : %d, buff_offset : %d, read_ptr : %d\n", i, filep->ppipe->ppipe_per_proc[i].active_read, filep->ppipe->ppipe_per_proc[i].buff_len, filep->ppipe->ppipe_per_proc[i].read_ptr);
    }

    ret_fd = -EINVAL; 
    if(filep->ppipe->ppipe_per_proc[process_index].open_write)
    {
        int start_pos = filep->ppipe->ppipe_global.write_ptr;
        count = (count >= filep->ppipe->ppipe_global.global_aval_space)? filep->ppipe->ppipe_global.global_aval_space: count;

        for(int i = 0; i < count; i++)
        {
            filep->ppipe->ppipe_global.ppipe_buff[(i+start_pos) % MAX_PPIPE_SIZE] = buff[i];
        }

        for(int i = 0; i < MAX_PPIPE_PROC; i++)
        {
            if(filep->ppipe->ppipe_per_proc[i].pid != -1)
            {
                filep->ppipe->ppipe_per_proc[i].buff_len += count;
            }
        }

        filep->ppipe->ppipe_global.write_ptr = (start_pos + count) % MAX_PPIPE_SIZE;
        filep->ppipe->ppipe_global.global_aval_space -= count;
        return count;
    }
    return ret_fd;

}

// Function to create persistent pipe.
int create_persistent_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of PPipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function.
     *  Create ppipe_info object by invoking the alloc_ppipe_info() function and
     *      fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *      -ENOMEM: If memory is not enough.
     *      -EOTHERS: Some other errors.
     *
     */
    
    // Simple return.

    int ret_val = -EOTHERS;
    if (!fd || !current) return ret_val;

    struct file* file1 = alloc_file();
    struct file* file2 = alloc_file();
    struct ppipe_info* ppipe1 = alloc_ppipe_info();
    
    file1->ppipe = ppipe1;
    file2->ppipe = ppipe1;

    file1->offp = 0;
    file2->offp = 0;

    file1->ref_count = 1;
    file2->ref_count = 1;

    file1->inode = NULL;
    file2->inode = NULL;

    file1->type = PPIPE;
    file2->type = PPIPE;

    file1->mode = O_READ;
    file2->mode = O_WRITE;

    file1->fops->read = ppipe_read;
    file2->fops->write = ppipe_write;

    file1->fops->close = ppipe_close;
    file2->fops->close = ppipe_close;

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
        if(fd[1] > MAX_OPEN_FILES) return -ENOMEM;
    }

    current->files[fd[1]]=file2;
    
    return 0;
}