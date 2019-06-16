#ifndef __SDRV_COMM_H_
#define __SDRV_COMM_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/seq_file.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_sas.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_tcq.h>
#include <linux/version.h>

#include "sdrv.h"


extern int g_log_level;

#define SDRV_LOG_LEVEL_ERR	(5)
#define SDRV_LOG_LEVEL_INFO	(3)
#define SDRV_LOG_LEVEL_DBG	(1)


#define PREFIX "[%s():%d] "

#define sdrv_err(fmt, arg...)  \
    do { if(likely(SDRV_LOG_LEVEL_ERR >= g_log_level)) printk(KERN_ERR PREFIX fmt "\n", __FUNCTION__, __LINE__, ## arg); \
    } while (0)

#define sdrv_info(fmt, arg...)  \
    do { if(likely(SDRV_LOG_LEVEL_INFO >= g_log_level)) printk(KERN_NOTICE PREFIX fmt "\n", __FUNCTION__, __LINE__, ## arg); \
    } while (0)

#define sdrv_dbg(fmt, arg...)  \
    do { if(unlikely(SDRV_LOG_LEVEL_DBG >= g_log_level)) printk(KERN_DEBUG PREFIX fmt "\n", __FUNCTION__, __LINE__, ## arg); \
    } while (0)

#define sdrv_pin(ms, fmt, arg...)  \
        do { printk(KERN_NOTICE PREFIX fmt "\n", __FUNCTION__, __LINE__, ## arg); \
             msleep(ms); \
        } while (0)


#define SDRV_NULL_IO_LEVEL_0    0
#define SDRV_NULL_IO_LEVEL_1    1   //without poll_wake_up/read/write
#define SDRV_NULL_IO_LEVEL_2    2   //without poll_wake_up/read/write/queues' operation
#define SDRV_NULL_IO_LEVEL_3    3   //without poll_wake_up/read/write/queues' operation/data copying


/* for host info */
struct sdrv_host_info
{
    __u16 hba_id;
    __u16 host_id;
    atomic64_t cmds_cnt;            //count of handled commands
    spinlock_t splock;
    __u32 ref_cnt;

    struct Scsi_Host * scsi_host;
    
    struct list_head req_q;         //client should read cmd from this queue
    struct list_head rsp_q;         //waiting for client's response in this queue
    struct list_head retry_q;
    struct list_head free_q;        //free struct sdrv_scsi_cmd_req queue
    void * free_q_mem;
    
    wait_queue_head_t poll_wait_q;  //most ONE file can wait in this queue at the same time
    struct file * file;             //the file which takes over this host(means the file is in poll_wait_q)

    struct sdrv_hba_info * hba_info;

    __u64 cmd_sn_mask;
    __u64 cmd_sn;

    //void * io_map;                //todo
};

#define SDRV_HOST_LOCK(host_info)   spin_lock(&host_info->splock)
#define SDRV_HOST_UNLOCK(host_info) spin_unlock(&host_info->splock)
/* end for host info */



int sdrv_ioctl_init(void);
void sdrv_ioctl_exit(void);


int sdrv_add_host(struct sdrv_ioctl_host_info * add_host_info);
int sdrv_rmv_host(__u16 hba_id, __u16 host_id);
int sdrv_conn_host(struct file * file, __u16 hba_id, __u16 host_id);
int sdrv_add_dev(struct sdrv_ioctl_dev_info * dev_info);
int sdrv_rmv_dev(struct sdrv_ioctl_dev_info * dev_info);

struct sdrv_host_info * sdrv_get_host_info(__u16 hba_id, __u16 host_id);

void sdrv_remove_all_dev_and_host(void);

/*sizeof(struct sdrv_scsi_cmd_req)*SDRV_MAX_IO_DEPTH_PER_HOST <= 128k*/
struct sdrv_scsi_cmd_req
{
    struct list_head node;
    __u64 cmd_sn;
    struct scsi_cmnd * cmd;
    void (*done)(struct scsi_cmnd *);
};


struct sdrv_scsi_cmd_req * host_scmd_req_alloc(struct sdrv_host_info * host_info);
void host_scmd_req_free(struct sdrv_host_info * host_info, struct sdrv_scsi_cmd_req * scmd_req);
void sdrv_handle_scsi_cmd(struct scsi_cmnd * cmd, void (*done)(struct scsi_cmnd *));
void sdrv_disconn_host(struct file * file);
void sdrv_host_dev_init(void);

inline struct list_head * list_del_tail(struct list_head * head);
inline struct list_head * list_del_first(struct list_head * head);



#endif
