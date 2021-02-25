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
 * For thread:
 *  1) Need thread id to know which thread it is. Should be able to extract from task_struct 
 *  2) cid make it easier to identify which container belong to
 *  3) The pointer to the next thread within the container
 *  4) Maybe add a task struct to keep track the thread? But shouldn't be able to read it from current?
*/


/**
 * Deregister the task from the container.
 */
int resource_container_delete(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    // not found, return -1
    return -1;
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

    // copy_from_user return 0 if it is success
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        printk(KERN_ERR "copy from user function fail from resource container create");
        return -1;
    }

    // copy from write success, cmd contain the cid from the user
    
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
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    return 0;
}
/**
 * Allocates memory in kernal space for sharing with tasks in the same container and 
 * maps the virtual address to the physical address.
 */
int resource_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    return ret;

}

/**
 * lock the container that is register by the current task.
 */
int resource_container_lock(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    return 0;
}

/**
 * unlock the container that is register by the current task.
 */
int resource_container_unlock(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    return 0;
}




/**
 * clean the content of the object in the container that is register by the current task.
 */
int resource_container_free(struct resource_container_cmd __user *user_cmd)
{
    struct resource_container_cmd cmd;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
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
