//////////////////////////////////////////////////////////////////////
//                     University of California, Riverside
//
//
//
//                             Copyright 2021
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for CSE202's Resource Container Project
//
////////////////////////////////////////////////////////////////////////

#include "resource_container.h"

//#include <asm/uaccess.h>
#include <asm/segment.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

/**
 * Idea for data structure:
 * For container:
 *  1) Need the cid for the container
 *  2) Need a pointer that point to the data structure of the thread
 *      2a) Might add another one to point to the end of the process, make it easier for adding another thread to the existing container
 *  3) Need a pointer to point to the next container.
 *      3a) if current container is the only one, point back to itself
 * 
 * First container:
 *  1) Need a pointer to the first container
 *  2) Will be NULL in the beginning if no thread create container yet
 *  3) After the first container is create, this variable will only change if the first container need to delete
 *  
 * For thread:
 *  1) Need thread id to know which thread it is. Should be able to extract from task_struct 
 *  2) cid make it easier to identify which container belong to
 *  3) The pointer to the next thread within the container
 *  4) Maybe add a task struct to keep track the thread? But shouldn't be able to read it from current?
*/
typedef struct thread_block thread_block;
typedef struct container_block container_block;
typedef struct thread_block{
    int cid;    //container id, use for debugging
    int tid;    //thread id, use to search the thread when delete is call
    thread_block* next_thread;      //pointer to the next thread block
    //next thread will be null if it is the last thread
    thread_block* prev_thread;      //pointer to the prev thread block
    struct task_struct* task_info;  //Use to load the info from current when create   
} thread_block;

typedef struct container_block{
    int cid;                        //container id
    thread_block* first_thread;     //point to the first thread for the container 
    thread_block* last_thread;      //point to the last thread for the container, [question] is it needed
    thread_block* running_thread;   //point to the threade that is running, use for switching
    container_block* next_container;    //point to the next container
    //next container will be null if it is the last container
    container_block* prev_container;    //point to the previous container
} container_block;

container_block* first_container = NULL;        //Use to check the first container
container_block* last_container = NULL;         //use to check the last container
container_block* switch_target_container = NULL;    //Use to see which container to do the switch


////////////////////////support function///////////////////////////////

// search_container_create: use to search for does a container exist, use by create function
// input: cid  -  the id for the container
// output: NULL if the container does not esist, else the target's container address.
container_block* search_container_create(int cid){
    container_block* temp;
    //debug statement
    printk("Called search container create, support for create function\n");
    // no container
    if(first_container == NULL){
        //debug statement
        printk("    search container create return: no container\n");
        return NULL;
    }
    
    temp = first_container;

    //continue to iterate all the container
    while(temp != NULL){
        if(temp->cid == cid){
            //debug statement
            printk("    search container create return: find container\n");
            return temp;            //if the current search container match the cid, return the address of that container block
        }
        temp = temp->next_container;
    }
    //debug statement
    printk("    search container create return: no container\n");
    return NULL;            //if iterate all the container but cannot find the target container, return NULL
}

// new_container_create: use to actually create the container and update the structure
// input: cid
// output: newly created container_block pointer
container_block* new_container_create(int cid){
    container_block* new_container = (container_block *)kmalloc(sizeof( container_block ) , GFP_KERNEL);    //allocate space for new container, use GFP_KERNEL because it should only be access by kernel 
    //input basic information for the new container block

    //debug statement
    printk("Called new container create, support for create function\n");
    new_container->cid = cid;
    new_container->first_thread = NULL;
    new_container->next_container = NULL;
    new_container->last_thread = NULL;
    new_container->running_thread = NULL;
    new_container->prev_container = NULL;
    //if it is the first container created, update first_container and switch_target_container
    if(first_container == NULL){
        first_container = new_container;
        last_container = new_container;
        switch_target_container = new_container;
    }
    else{       //else, update the last container
        last_container->next_container = new_container;
        new_container->prev_container = last_container;
        last_container = new_container;
        
    }
    //debug statement
    printk("    new_container_create return: new container\n");
    return new_container;
}

// new_thread_create: create new thread structure and connect to the container block
thread_block* new_thread_create(container_block* cblock){

    thread_block* temp = cblock->first_thread;
    thread_block* new_thread = (thread_block *)kmalloc(sizeof( thread_block ) , GFP_KERNEL);        //allocate space for thread_block

    //debug statement
    printk("Called new thread create, support for create\n");
    new_thread->task_info = current;
    new_thread->cid = cblock->cid;
    new_thread->next_thread = NULL;
    new_thread->tid = current->pid;
    new_thread->prev_thread = NULL;
    // if first thread is NULL, need to update first thread and last thread to new thread that just created
    // And this thread will be the running thread
    if(temp == NULL){
        cblock->first_thread = new_thread;
        cblock->last_thread = new_thread;
        cblock->running_thread = new_thread;        
    }
    // else, it already has at least a thread in the thread list, update the original last thread to point to the new last thread, and update container last thread
    // Also need to sleep the thread
    else{
        cblock->last_thread->next_thread = new_thread;
        new_thread->prev_thread = cblock->last_thread;
        cblock->last_thread = new_thread;        
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
    }
    //debug statement
    printk("    new_thread_create return: new thread\n");
    return new_thread;
}

// find_tid: search a container to see does it contain the thread with certain tid
thread_block* find_tid(int tid, container_block* cblock){
    thread_block* temp = cblock->first_thread;

    //debug statement
    printk("Called find tid function, support for create and delete\n");

    if(temp == NULL){       //should not happen
        printk(KERN_ERR "copy from user function fail from find tid\n");
        return NULL;
    }
    while(temp != NULL){         //check all the thread block other than last thread block
        if(temp->tid == tid){                   //if find, return true
            //debug statement
            printk("    find_tid return: find thread\n");
            return temp;
        }
        else{                                   //else, continue to search next thread block
            temp = temp->next_thread;
        }
    }
    //debug statement
    printk("    find_tid return: cannot find thread\n");
    return NULL;

}

//search_all_container_tid: search all the container to see if any contain a thread with target tid
//return contain_block if find, else return NULL
container_block* search_all_container_tid(int tid){
    container_block* temp = first_container;

    //debug statement
    printk("Called search all container tid function, support for main create\n");

    if(temp == NULL){       //should not happen
        printk(KERN_ERR "copy from user function fail from find tid\n");
        return NULL;
    }

    while(temp != NULL){
        if(find_tid(tid,temp) != NULL){
            //debug statement
            printk("    search_all_container_tid return: find container\n");
            return temp;
        }
        else{
            temp = temp->next_container;
        }
    }
    //debug statement
    printk("    search_all_container_tid return: not find container\n");
    return NULL;
}


// thread_remove: use as support for delete a thread, need to have the container that contain the thread as input
int thread_remove(int tid, container_block* cblock){
    container_block* prev = NULL;
    container_block* next = NULL;
    thread_block* temp = NULL;

    //debug statement
    printk("Called thread remove function, support for main delete\n");

    //case 1: only 1 thread within the cblock
    if(cblock->first_thread == cblock->last_thread){
        temp = cblock->first_thread;

        if(temp->tid != tid){   //check is the tid match, if not, return -1
            printk(KERN_ERR "seomething wrong with thread remove\n");
            return -1;
        }

        //prepare to remove the container
        prev = cblock->prev_container;
        next = cblock->next_container;

        if(prev == NULL && next == NULL){       //if this is the only container
            first_container = NULL;
            last_container = NULL;
            switch_target_container = NULL;
        }
        else if(prev == NULL){                  //if current container is the first one
            first_container = next;
            next->prev_container = NULL;
        }
        else if(next == NULL){                  //if current container is the last one
            prev->next_container = NULL;
            last_container = prev;
        }
        else{                                   //if it is a middle container
            prev->next_container = next;
            next->prev_container = prev;
        }
        
        printk("removing thread\n");
        kfree(temp);
        printk("removing container\n");
        kfree(cblock);
        //debug statement
        printk("    thread_remove return: case 1 success\n");
        return 0;
    }

    //case 2: more than 1 thread within the cblock
    else{
        thread_block* temp = find_tid(tid,cblock);

        thread_block* prev = temp->prev_thread;
        thread_block* next = temp->next_thread;

        if(prev == NULL && next == NULL){       // case when it is the only thread, should not happen
            printk(KERN_ERR "wrong with case 2 in thread remove\n");
            return -1;
        }
        else if(prev == NULL){                  //current thread is the first thread
            cblock->first_thread = next;
            next->prev_thread = NULL;
        }
        else if(next == NULL){                  //current thread is the last thread
            cblock->last_thread = prev;
            prev->next_thread = NULL;
        }
        else{                                   //middle case for thread
            prev->next_thread = next;
            next->prev_thread = prev;
        }

        printk("removing thread\n");
        kfree(temp);
        //debug statement
        printk("    thread_remove return: case 2 success\n");
        return 0;
    }
}

// print_all_container_thread: use for debug, print all the container and thread
void print_all_container_thread(void){
    container_block* temp_container;
    thread_block* temp_thread;
    printk("Start to print all the container and thread:\n");
    
    if(first_container == NULL){
        printk("    No container exist\n");
        return;
    }

    temp_container = first_container;

    while(temp_container != NULL){
        printk("    cid = %d", temp_container->cid);
        //check first thread
        if(temp_container->first_thread == NULL){
            printk("    first thread NULL");
        }
        else{
            printk("    first thread exist");
        }
        //check last thread
        if(temp_container->last_thread == NULL){
            printk("    last thread NULL");
        }
        else{
            printk("    last thread exist");
        }
        //check running thread
        if(temp_container->running_thread == NULL){
            printk("    running thread NULL");
        }
        else{
            printk("    running thread exist");
        }
        //check next container
        if(temp_container->next_container == NULL){
            printk("    next_container NULL");
        }
        else{
            printk("    next_container exist");
        }
        //check prev container
        if(temp_container->prev_container == NULL){
            printk("    prev_container NULL");
        }
        else{
            printk("    prev_container exist");
        }
        printk("\n");

        temp_thread = temp_container->first_thread;
        if(temp_thread == NULL){
            printk(KERN_ERR "container is empty but not deleted\n");
        }

        while(temp_thread != NULL){
            printk("        tid = %d", temp_thread->tid);
            printk("        cid = %d", temp_thread->cid);
            //check next thread
            if(temp_thread->next_thread == NULL){
                printk("        next_thread NULL");
            }
            else{
                printk("        next_thread exist");
            }
            //check next thread
            if(temp_thread->prev_thread == NULL){
                printk("        prev_thread NULL");
            }
            else{
                printk("        prev_thread exist");
            }
            printk("\n");
            temp_thread = temp_thread->next_thread;
        }
        temp_container = temp_container->next_container;
    }
    printk("finish printing\n\n\n");
    return;

}

/**
 * Deregister the task from the container.
 * user_cmd does not contain useful information
 * Need to use current to get the tid and search from it
 */
int resource_container_delete(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    int target_tid = 0;
    container_block* temp_container = NULL;

    //debug statement
    printk("Called main delete function\n");
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        printk(KERN_ERR "copy from user function fail from resource container delete\n");
        return -1;
    }

    // Need to find the current thread information and remove it from the container
    target_tid = current->pid;       //find the current thread pid

    if(first_container == NULL){    //no container case
        printk(KERN_ERR "copy from user function fail from resource container create\n");
        return -1;
    }

    temp_container = search_all_container_tid(target_tid);

    if(temp_container == NULL){
        printk(KERN_ERR "Not find thread in anywhere in the kernel\n");
        return -1;
    }

    if(thread_remove(target_tid,temp_container) == -1){
        printk(KERN_ERR "error in removing thread\n");
        return -1;
    }
    //debug statement
    printk("    resource_container_delete return: sucess delete\n");
    print_all_container_thread();
    return 0;
}

/**
 * Create/Assign a task to the corresponding container.
 * Idea:
 * 1) Need to get the container id (which is cid) from the user
 * 2) cid can be use to check do I already have the container already, if so, add the new process to the container
 * 3) if new cid, then first create the container and add the new process to the container
 * 4) Need a way to keep track the container number that is loaded
 *      4a) might be good to use link list?
 * 5) A container can also has multiple process within, need another data structer to list all the process within a container
 *      5a) might be also good to use link list?
 * 
 * Step:
 * 1) extract the cid from user space cmd, check if there is a container for the cid
 *  1a) If not, create the container, and put the thread into the new container
 *  1b) If yes, append the thread into the container thread list
 *  
 */
int resource_container_create(struct resource_container_cmd __user *user_cmd)
{
    // Structure use to keep the information that is pass from the user
    struct resource_container_cmd cmd;
    container_block* temp = NULL;

    //debug output
    printk("Called main create function\n");


    // copy_from_user return 0 if it is success
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        printk(KERN_ERR "copy from user function fail from resource container create\n");
        return -1;
    }

    // copy from write success, cmd contain the cid from the user
    temp = search_container_create(cmd.cid);      //search does a container block exist already

    if(temp == NULL){           //case when target container does not exist
        temp = new_container_create(cmd.cid);
    }

    //Now temp has the pointer to the target continer, need to add the new thread to the container
    new_thread_create(temp);
    //debug statement
    printk("    resource_container_create return: sucess create\n");    
    print_all_container_thread();
    return 0;
}

/**
 * Switch idea: Need to keep it simple. Each of the switch only apply to one container.
 *  If current selected container only has one thread, switch do nothing for the selected container.
 *  If the container has more than one thread, make the current runable thread sleep and wake the next thread.
 * At the end for switch case, update the container pointer to point to the next container to get ready for next switch
 * 
 * Downside for this approch:
 *  Not scalable, as the number of container increase, switch happen in each container decrease
 *  
 * Pro:
 *  Seem to be not too hard to implement
 * 
*/

int resource_container_switch(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    //debug statement
    printk("resource_container_switch start\n");  
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    //debug statement
    printk("resource_container_switch end\n"); 
    return 0;
}
/**
 * Allocates memory in kernal space for sharing with tasks in the same container and 
 * maps the virtual address to the physical address.
 */
int resource_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    container_block* temp_container;

    //remap_pfn_range can be use?
    //debug statement
    printk("resource_container_mmap start\n"); 

    temp_container = search_all_container_tid(current->pid);

    if(temp_container == NULL){
        printk("container not found with pid: %d", current->pid);
    }
    else{
        printk("The cid for container is %d", temp_container->cid);
    }

    printk("The vma page offset value is: %lu", vma->vm_pgoff);

    //debug statement
    printk("resource_container_mmap end\n"); 
    return ret;

}

/**
 * lock the container that is register by the current task.
 */
int resource_container_lock(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    //debug statement
    printk("resource_container_lock start\n"); 
    
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    //debug statement
    printk("resource_container_lock end\n"); 
    return 0;
}

/**
 * unlock the container that is register by the current task.
 */
int resource_container_unlock(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    //debug statement
    printk("resource_container_unlock start\n"); 
    
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    //debug statement
    printk("resource_container_unlock end\n"); 
    return 0;
}




/**
 * clean the content of the object in the container that is register by the current task.
 */
int resource_container_free(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    //debug statement
    printk("resource_container_free start\n"); 
    
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    //debug statement
    printk("resource_container_free end\n"); 
    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */

int resource_container_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case RCONTAINER_IOCTL_CSWITCH:
        return resource_container_switch((void __user *) arg);
    case RCONTAINER_IOCTL_CREATE:
        return resource_container_create((void __user *) arg);
    case RCONTAINER_IOCTL_DELETE:
        return resource_container_delete((void __user *) arg);
    case RCONTAINER_IOCTL_LOCK:
        return resource_container_lock((void __user *)arg);
    case RCONTAINER_IOCTL_UNLOCK:
        return resource_container_unlock((void __user *)arg);
    case RCONTAINER_IOCTL_FREE:
        return resource_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
