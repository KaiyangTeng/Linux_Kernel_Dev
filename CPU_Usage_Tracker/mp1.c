// SPDX-License-Identifier: GPL-2.0-only
/*
 * This module emits "Hello, world" on printk when loaded.
 *
 * It is designed to be used for basic evaluation of the module loading
 * subsystem (for example when validating module signing/verification). It
 * lacks any extra dependencies, and will not normally be loaded by the
 * system unless explicitly requested by name.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>  
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>      
#include <linux/sched.h>    
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include "mp1_given.h"

// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
// Please put your name and email here
MODULE_AUTHOR("Kaiyang Teng <kteng4@illinois.edu>");
MODULE_LICENSE("GPL");

static struct proc_dir_entry *mp1dir;
static struct proc_dir_entry *mp1status;
static struct mutex mp1mutex;
static struct list_head mp1listhead;           
static struct kmem_cache *mp1cache;


struct listnode
{
	pid_t pid;
    unsigned long cpu_time;
	struct list_head klistnode;
};

static struct timer_list timer;
static struct work_struct worker;

static void work_func(struct work_struct *worker)
{
	struct listnode *cur,*pad;
	unsigned long temptime;
	mutex_lock(&mp1mutex);
	list_for_each_entry_safe(cur,pad,&mp1listhead,klistnode)
	{
		if(get_cpu_use(cur->pid,&temptime)==-1)
		{
			list_del(&cur->klistnode);
			kmem_cache_free(mp1cache,cur);
		}
		else cur->cpu_time=temptime;
	}
	mutex_unlock(&mp1mutex);
}

static void timercb(struct timer_list *intimer)
{
	schedule_work(&worker);
    mod_timer(&timer,jiffies+5*HZ);
}


static int idregister(pid_t pid)
{
	struct listnode *node,*temp;
	node=kmem_cache_alloc(mp1cache, GFP_KERNEL);
	if(!node) return -ENOMEM;
	node->pid=pid;
	node->cpu_time=0;
	mutex_lock(&mp1mutex);
	list_for_each_entry(temp,&mp1listhead,klistnode)
	{
		if(temp->pid==pid)
		{
			mutex_unlock(&mp1mutex);
			kmem_cache_free(mp1cache,node);
			return -EEXIST;
		}
	}
	list_add_tail(&node->klistnode,&mp1listhead);
	mutex_unlock(&mp1mutex);
	return 0;
}


ssize_t my_read(struct file * inputfile, char __user * usrbuf, size_t len, loff_t * p)
{
	struct listnode *cur;
	char *kbuf;
	int total=0,left=0,cap=10240;
	kbuf=kmalloc(cap,GFP_KERNEL);
	if(!kbuf) return -ENOMEM;
	mutex_lock(&mp1mutex);
	list_for_each_entry(cur,&mp1listhead,klistnode)
	{
		if(total>=cap-16) break;
		total+=scnprintf(kbuf+total,cap-total,"%d: %lu\n",cur->pid,cur->cpu_time);
	}
	mutex_unlock(&mp1mutex);
	if(*p>=total) 
	{
		kfree(kbuf);
		return 0;
	}
	left=total-*p;
	if(left<len) len=left;
	if(copy_to_user(usrbuf,kbuf+*p,len)) 
	{
		kfree(kbuf);
		return -EFAULT;
	}
	*p+=len;
	kfree(kbuf);
	return len;
}

ssize_t my_write(struct file * inputfile, const char __user *usrbuf, size_t len, loff_t * p)
{
	char kbuf[16];
	pid_t pid;
	int res;
	if(len==0||len>=sizeof(kbuf)) return -EINVAL;
	if(copy_from_user(kbuf,usrbuf,len)!=0) return -EFAULT;
	kbuf[len]='\0';
	strim(kbuf); 
	if(kstrtoint(kbuf,10,&pid)!=0) return -EINVAL;
	res=idregister(pid);
	if(res) return res;
	return len;
}

static const struct proc_ops my_ops=
{
	.proc_read=my_read,
	.proc_write=my_write,
};


static void cleanuplist(void)
{
	struct listnode *cur,*pad;
	mutex_lock(&mp1mutex);
	list_for_each_entry_safe(cur,pad,&mp1listhead,klistnode)
	{
		list_del(&cur->klistnode);
		kmem_cache_free(mp1cache,cur);
	}
	mutex_unlock(&mp1mutex);
}


static int __init test_module_init(void)
{
	mp1dir=proc_mkdir("mp1",NULL);
	if(!mp1dir) return -ENOMEM;
	mp1status=proc_create("status",0666,mp1dir,&my_ops);
	if(!mp1status)
	{
		proc_remove(mp1dir);
        mp1dir = NULL;
        return -ENOMEM;
	}
	mutex_init(&mp1mutex);
	INIT_LIST_HEAD(&mp1listhead);
	mp1cache=kmem_cache_create("cech4mp1",sizeof(struct listnode),0,SLAB_HWCACHE_ALIGN,NULL);
	if(!mp1cache) 
	{
        proc_remove(mp1status);
        mp1status = NULL;
        proc_remove(mp1dir);
        mp1dir = NULL;
        return -ENOMEM;
    }
	INIT_WORK(&worker,work_func);
	timer_setup(&timer,timercb,0);
	mod_timer(&timer,jiffies+5*HZ);
	return 0;
}

module_init(test_module_init);

static void __exit test_module_exit(void)
{
	del_timer_sync(&timer);
	flush_work(&worker);
	cleanuplist();
	mutex_destroy(&mp1mutex);
	if(mp1cache) 
	{
		kmem_cache_destroy(mp1cache);
		mp1cache=NULL;
	}
	if(mp1status) 
	{
        proc_remove(mp1status);
        mp1status=NULL;
    }
    if(mp1dir) 
	{
        proc_remove(mp1dir);
        mp1dir=NULL;
    }
}

module_exit(test_module_exit);
