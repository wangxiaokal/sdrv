#include "sdrv_comm.h"


#define QUEUECOMMAND_WITH_DONE_FN

#define SDRV_NULL_IO

extern int g_sdrv_null_io;

/* for hba mgr */
struct sdrv_hba_mgr
{
    __u16 hba_num;
    struct list_head hba_list;
    spinlock_t splock;
};

#define SDRV_HBA_MGR_LOCK()     spin_lock(&g_sdrv_hba_mgr.splock)
#define SDRV_HBA_MGR_UNLOCK()   spin_unlock(&g_sdrv_hba_mgr.splock)

#define SDRV_MAX_HBA_NUM        (16)
/* end for hba mgr */


/* for hba info */
#define SDRV_MAX_HOST_PER_HBA   (16)

struct sdrv_hba_info
{
    struct list_head node;
    __u16 hba_id;
    __u16 host_num;
    void * hosts[SDRV_MAX_HOST_PER_HBA];    //host can only be removed when there is no device in this hba
};

/* end for hba info */


struct sdrv_hba_mgr g_sdrv_hba_mgr;

void sdrv_host_dev_init(void)
{
    g_sdrv_hba_mgr.hba_num = 0;
    INIT_LIST_HEAD(&g_sdrv_hba_mgr.hba_list);
    spin_lock_init(&g_sdrv_hba_mgr.splock);

    return;
}

static inline __u64 sdrv_get_scmd_sn(struct sdrv_host_info * host_info)
{
    return ((host_info->cmd_sn++ | 0xffff000000000000) & host_info->cmd_sn_mask);
}

static inline struct sdrv_host_info * sdrv_assign_host(struct sdrv_host_info * host_info)
{
    struct sdrv_host_info * ret_host;
    __u64 cmd_cnt = atomic64_inc_return(&host_info->cmds_cnt);

    struct sdrv_hba_info * hba_info = host_info->hba_info;
    BUG_ON(!hba_info->host_num);
    
    //host can only be removed when there is no device in hba, so we don't need lock here
    ret_host = hba_info->hosts[cmd_cnt%hba_info->host_num];
    if(likely(ret_host))
        return ret_host;
    else
        return host_info;
}

static int sdrv_scsi_queue_command_with_done_fn(struct scsi_cmnd * cmd, void (*done)(struct scsi_cmnd *))
{
    struct list_head * node;
    struct sdrv_host_info *host_info, *orig_host = *(struct sdrv_host_info **)cmd->device->host->hostdata;
	struct sdrv_scsi_cmd_req * scmd_req;

    /*
	unsigned char cmd_type = cmd->cmnd[0];
    sdrv_info("[SCSI CMD] cmnd:%#x %#x seq_no:%lu cmd_len:%u dir:%u sdb_len:%u sdb_resid:%u first_sgl:%p nents:%u orig_nents:%u",
        cmd_type, cmd->cmnd[1], cmd->serial_number, cmd->cmd_len, cmd->sc_data_direction,
        cmd->sdb.length, cmd->sdb.resid, cmd->sdb.table.sgl, cmd->sdb.table.nents, cmd->sdb.table.nents);
    */

#ifdef SDRV_NULL_IO
    if(g_sdrv_null_io == SDRV_NULL_IO_LEVEL_2 || g_sdrv_null_io == SDRV_NULL_IO_LEVEL_3)    //without queues' operation
    {
        sdrv_handle_scsi_cmd(cmd, done);
        return 0;
    }
#endif

    host_info = sdrv_assign_host(orig_host);

    SDRV_HOST_LOCK(host_info);
    scmd_req = host_scmd_req_alloc(host_info);
    if(unlikely(!scmd_req))
    {
        SDRV_HOST_UNLOCK(host_info);
        sdrv_err("alloc scsi_cmd_req failed");
        cmd->result = 0x01; //unit attention
        done(cmd);
        return 0;
    }

    scmd_req->cmd_sn = sdrv_get_scmd_sn(host_info);
    scmd_req->cmd = cmd;
    scmd_req->done = done;

    list_add_tail(&scmd_req->node, &host_info->req_q);
    
    SDRV_HOST_UNLOCK(host_info);

#ifdef SDRV_NULL_IO
    if(g_sdrv_null_io == SDRV_NULL_IO_LEVEL_1)  //without poll_wake_up/read/write
    {
        SDRV_HOST_LOCK(host_info);
        node = list_del_first(&host_info->req_q);
        list_add_tail(node, &host_info->rsp_q);
        SDRV_HOST_UNLOCK(host_info);

        SDRV_HOST_LOCK(host_info);
        node = list_del_first(&host_info->rsp_q);
        scmd_req = list_entry(node, struct sdrv_scsi_cmd_req, node);
        cmd = scmd_req->cmd;
        done = scmd_req->done;
        host_scmd_req_free(host_info, scmd_req);
        SDRV_HOST_UNLOCK(host_info);

        sdrv_handle_scsi_cmd(cmd, done);
    }
    else
#endif
        wake_up(&host_info->poll_wait_q);

    return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
static int sdrv_scsi_queue_command(struct Scsi_Host * shost, struct scsi_cmnd * cmd)
{
    return sdrv_scsi_queue_command_with_done_fn(cmd, cmd->scsi_done);
}
#endif


static struct scsi_host_template g_sdrv_scsi_host_template =
{
    .module                   = THIS_MODULE,
    .name                     = "sdrv",
    .proc_name                = "sdrv",
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
    .queuecommand             = sdrv_scsi_queue_command_with_done_fn,
#else
	.queuecommand             = sdrv_scsi_queue_command,
#endif
    .this_id                  = -1,
    .can_queue                = 1,
    .max_sectors              = 2048,   //max sectors for single scsi cmd, 1M
    .sg_tablesize             = 128,    //jusan liebiao nengli
    .use_clustering            = ENABLE_CLUSTERING, //merge seq io
    //.bios_param               = sdrv_bios_param,
    //.eh_abort_handler         = sdrv_eh_abort_handler,
    //.eh_device_reset_handler  = sdrv_eh_device_reset_handler,
    //.change_queue_depth       = sdrv_change_queue_depth,
    //.slave_alloc              = sdrv_slave_alloc,
    //.slave_configure          = sdrv_slave_configure,
    //.scan_finished            = sdrv_scan_finished,
    //.scan_start               = sdrv_scan_start,
};

void sdrv_host_info_init(struct sdrv_host_info * host_info)
{
    if(unlikely(!host_info)) return;

    host_info->hba_id = host_info->host_id = 0;
    atomic64_set(&host_info->cmds_cnt, 0);
    spin_lock_init(&host_info->splock);
    host_info->ref_cnt = 0;

    host_info->scsi_host = NULL;
    
    INIT_LIST_HEAD(&host_info->req_q);
    INIT_LIST_HEAD(&host_info->rsp_q);
    INIT_LIST_HEAD(&host_info->retry_q);
    INIT_LIST_HEAD(&host_info->free_q);
    host_info->free_q_mem = NULL;
    
    init_waitqueue_head(&host_info->poll_wait_q);
    host_info->file = NULL;

    host_info->hba_info = NULL;

    host_info->cmd_sn = host_info->cmd_sn_mask = 0;
}

int sdrv_host_free_q_init(struct sdrv_host_info * host_info)
{
    __u32 i;
    struct sdrv_scsi_cmd_req * cmd_mem;
    
    if(unlikely(!host_info)) return -EINVAL;

    if(!list_empty(&host_info->free_q) || host_info->free_q_mem)
    {
        sdrv_err("free_q not empty or free_q_mem %p is not null", host_info->free_q_mem);
        return -EEXIST;
    }

    cmd_mem = kmalloc(sizeof(struct sdrv_scsi_cmd_req)*SDRV_MAX_IO_DEPTH_PER_HOST, GFP_KERNEL);
    if(!cmd_mem)
    {
        sdrv_err("vmalloc mem failed, size=%lu", sizeof(struct sdrv_scsi_cmd_req)*SDRV_MAX_IO_DEPTH_PER_HOST);
        return -ENOMEM;
    }

    for(i = 0; i < SDRV_MAX_IO_DEPTH_PER_HOST; i++)
    {
        cmd_mem[i].cmd_sn = 0;
        cmd_mem[i].cmd = NULL;
        cmd_mem[i].done = NULL;
        list_add_tail(&cmd_mem[i].node, &host_info->free_q);
    }

    host_info->free_q_mem = cmd_mem;

    return 0;
}

void sdrv_host_free_q_destroy(struct sdrv_host_info * host_info)
{
    if(unlikely(!host_info)) return;

    INIT_LIST_HEAD(&host_info->free_q);
    if(likely(host_info->free_q_mem))
    {
        kfree(host_info->free_q_mem);
        host_info->free_q_mem = NULL;
    }

    return;
}

void sdrv_host_info_cmd_sn_mask_init(struct sdrv_host_info * host_info)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    host_info->cmd_sn_mask = (((__u64)tv.tv_usec) << 48) | 0x0000ffffffffffff;
}

struct sdrv_hba_info * sdrv_get_hba_info(__u16 hba_id)
{
    struct sdrv_hba_info * hba_info;
    struct list_head * node;
    
    list_for_each(node, &g_sdrv_hba_mgr.hba_list)
    {
        hba_info = list_entry(node, struct sdrv_hba_info, node);
        if(hba_info->hba_id == hba_id)
        {
            return hba_info;
        }
    }
    
    return NULL;
}

struct sdrv_host_info * sdrv_get_host_info(__u16 hba_id, __u16 host_id)
{
    __u32 i;
    struct sdrv_hba_info * hba_info;
    struct sdrv_host_info * host_info;

    hba_info = sdrv_get_hba_info(hba_id);
    if(unlikely(!hba_info))
    {
        sdrv_err("hba[%u] not found", hba_id);
        return NULL;
    }
    
    for(i = 0; i < hba_info->host_num; i++)
    {
        host_info = hba_info->hosts[i];
        BUG_ON(!host_info);

        if(host_info->host_id == host_id)
        {
            BUG_ON(!host_info->scsi_host);
            return host_info;
        }
    }
    
    return NULL;
}

void sdrv_host_info_free(struct sdrv_host_info * host_info)
{
    if(likely(host_info))
    {
        sdrv_host_free_q_destroy(host_info);
        sdrv_host_info_init(host_info);
        kfree(host_info);
    }
    
    return;
}

struct sdrv_host_info * sdrv_host_info_alloc(void)
{
    struct sdrv_host_info * host_info = kmalloc(sizeof(struct sdrv_host_info), GFP_KERNEL);
    if(unlikely(!host_info))
    {
        sdrv_err("kmalloc host_info failed");
        return NULL;
    }
    
    sdrv_host_info_init(host_info);
    sdrv_host_info_cmd_sn_mask_init(host_info);
    if( unlikely( sdrv_host_free_q_init(host_info) ) )
    {
        sdrv_host_info_free(host_info);
        return NULL;
    }
    
    return host_info;
}

inline struct sdrv_scsi_cmd_req * host_scmd_req_alloc(struct sdrv_host_info * host_info)
{
    //node should be the first member of struct sdrv_scsi_cmd_req
    return (struct sdrv_scsi_cmd_req *)list_del_tail(&host_info->free_q);
}

inline void host_scmd_req_free(struct sdrv_host_info * host_info, struct sdrv_scsi_cmd_req * scmd_req)
{
    list_add_tail(&scmd_req->node, &host_info->free_q);
    return;
}

int sdrv_add_hba(__u16 hba_id)
{
    struct sdrv_hba_info * hba_info = kzalloc(sizeof(struct sdrv_hba_info), GFP_KERNEL);
    if(unlikely(!hba_info))
    {
        sdrv_err("alloc hba_info for hba[%u] failed", hba_id);
        return -ENOMEM;
    }

    INIT_LIST_HEAD(&hba_info->node);
    hba_info->hba_id = hba_id;
    
    list_add_tail(&hba_info->node, &g_sdrv_hba_mgr.hba_list);
    g_sdrv_hba_mgr.hba_num++;
    
    return 0;
}

void __check_hba_hosts(struct sdrv_hba_info * hba_info)
{
    __u32 i;

    if(unlikely(!hba_info)) return;
    
    for(i = 0; i < SDRV_MAX_HOST_PER_HBA; i++)
    {
        if(i < hba_info->host_num)
            BUG_ON(!hba_info->hosts[i]);
        else
            BUG_ON(hba_info->hosts[i]);
    }
}

void sdrv_rmv_hba_if_no_host(__u16 hba_id)
{
    struct list_head * node;
    struct sdrv_hba_info * hba_info;
    
    list_for_each(node, &g_sdrv_hba_mgr.hba_list)
    {
        hba_info = list_entry(node, struct sdrv_hba_info, node);

        if(hba_info->hba_id == hba_id)
        {
            __check_hba_hosts(hba_info);
            
            if(hba_info->host_num == 0)
            {
                list_del(&hba_info->node);
                g_sdrv_hba_mgr.hba_num--;
                kfree(hba_info);
            }

            return;
        }
    }
}

//fill the add_host_info->sys_host_no if add host succeeded
int sdrv_add_host(struct sdrv_ioctl_host_info * add_host_info)
{
    struct sdrv_hba_info * hba_info = NULL;
    struct sdrv_host_info * host_info = NULL;
    struct Scsi_Host * sh = NULL;
    void ** p_pointer;
    int ret;
    __u32 sys_host_no;
    
    if(unlikely(!add_host_info))
    {
        sdrv_err("add_host_info %p null", add_host_info);
        return -EINVAL;
    }

    SDRV_HBA_MGR_LOCK();

    host_info = sdrv_get_host_info(add_host_info->hba_id, add_host_info->host_id);
    if(unlikely(host_info))
    {
        sys_host_no = host_info->scsi_host->host_no;
        SDRV_HBA_MGR_UNLOCK();

        add_host_info->sys_host_no = sys_host_no;
        sdrv_info("host[%u,%u-%u] already exist", sys_host_no, add_host_info->hba_id, add_host_info->host_id);
        return 0;
    }

    do
    {
        //if hba_info not exist, add hba_info first
        hba_info = sdrv_get_hba_info(add_host_info->hba_id);
        if(unlikely(!hba_info))
        {
            sdrv_info("host[%u-%u] hba not exist, add hba first", add_host_info->hba_id, add_host_info->host_id);

            ret = sdrv_add_hba(add_host_info->hba_id);
            if(ret)
            {
                sdrv_err("host[%u-%u] add hba failed, ret=%d", add_host_info->hba_id, add_host_info->host_id, ret);
                break;
            }

            hba_info = sdrv_get_hba_info(add_host_info->hba_id);
            BUG_ON(!hba_info);
        }
        
        if(unlikely(hba_info->host_num >= SDRV_MAX_HOST_PER_HBA))
        {
            sdrv_err("add host[%u-%u] failed: host_num %u in hba reaches max %u",
                    add_host_info->hba_id, add_host_info->host_id, hba_info->host_num, SDRV_MAX_HOST_PER_HBA);
            ret = -EFAULT;
            break;
        }
        
        host_info = sdrv_host_info_alloc();
        if(unlikely(!host_info))
        {
            sdrv_err("alloc host_info for host[%u-%u] failed", add_host_info->hba_id, add_host_info->host_id);
            ret = -ENOMEM;
            break;
        }
        host_info->hba_id = add_host_info->hba_id;
        host_info->host_id = add_host_info->host_id;
        host_info->hba_info = hba_info;
        
        sh = scsi_host_alloc(&g_sdrv_scsi_host_template, sizeof(void*));
        if(unlikely(!sh))
        {
            sdrv_err("alloc Scsi_Host for host[%u-%u] failed", add_host_info->hba_id, add_host_info->host_id);
            ret = -ENOMEM;
            break;
        }

        host_info->scsi_host = sh;

        p_pointer = (void**)shost_priv(sh);
        *p_pointer = host_info;

        /*set the host param*/
        sh->this_id = -1;
        sh->max_channel = add_host_info->max_channel_num;
        sh->max_id = add_host_info->max_target_num;
        sh->max_lun = add_host_info->max_lun_num;
        
        sh->max_cmd_len = add_host_info->max_cdb_len;
        sh->can_queue = add_host_info->max_cmd_num;
        sh->cmd_per_lun = add_host_info->max_cmds_per_lun;

        sh->sg_tablesize = add_host_info->sg_tablesize;


        if(unlikely( ret = scsi_add_host(sh, NULL) ))
        {            
            sdrv_err("add host[%u-%u] failed, ret=%d", add_host_info->hba_id, add_host_info->host_id, ret);
            break;
        }

        hba_info->hosts[hba_info->host_num++] = host_info;
        sys_host_no = sh->host_no;
    }while(0);

    if(ret)
    {
        sdrv_rmv_hba_if_no_host(add_host_info->hba_id);
        SDRV_HBA_MGR_UNLOCK();

        if(host_info) sdrv_host_info_free(host_info);
        if(sh) scsi_host_put(sh);
        return ret;
    }
    else
    {
        SDRV_HBA_MGR_UNLOCK();
        sdrv_info("add host[%u,%u-%u] succeeded", sys_host_no, add_host_info->hba_id, add_host_info->host_id);
        add_host_info->sys_host_no = sys_host_no;
        return 0;
    }
}

static inline bool __host_idle(struct sdrv_host_info * host_info)
{
    return (list_empty(&host_info->req_q) && list_empty(&host_info->rsp_q)
            && list_empty(&host_info->retry_q) && list_empty(&host_info->scsi_host->__devices));
}

//host can only be removed when there is no device in this hba
int sdrv_rmv_host(__u16 hba_id, __u16 host_id)
{
    __u32 i, sys_host_no = 0, host_idx = SDRV_MAX_HOST_PER_HBA;
    struct sdrv_host_info * host_info;
    struct sdrv_hba_info * hba_info;
    int ret = 0;

    SDRV_HBA_MGR_LOCK();

    do
    {
        host_info = sdrv_get_host_info(hba_id, host_id);
        if(unlikely(!host_info))
        {
            sdrv_info("host[%u-%u] not found", hba_id, host_id);
            break;
        }

        if(unlikely(host_info->ref_cnt))
        {
            sdrv_err("host[%u,%u-%u] ref_cnt is %u, can not remove it now",
                    host_info->scsi_host->host_no, hba_id, host_id, host_info->ref_cnt);
            ret = -EPERM;
            break;
        }

        hba_info = sdrv_get_hba_info(hba_id);
        BUG_ON(!hba_info || !hba_info->host_num);

        for(i = 0; i < hba_info->host_num; i++)
        {
            BUG_ON(!hba_info->hosts[i]);
            if(!__host_idle(hba_info->hosts[i]))
            {
                sdrv_err("host[%u-%u] not idle", hba_id, host_id);
                ret = -EAGAIN;
                break;
            }

            if(hba_info->hosts[i] == host_info) host_idx = i;
        }

        BUG_ON(host_idx == SDRV_MAX_HOST_PER_HBA);
        hba_info->hosts[host_idx] = NULL;

        for(i = host_idx; i < hba_info->host_num-1; i++)
        {
            hba_info->hosts[i] = hba_info->hosts[i+1];
        }
        hba_info->hosts[i] = NULL;
        hba_info->host_num--;
        
        sys_host_no = host_info->scsi_host->host_no;
        scsi_remove_host(host_info->scsi_host);
        scsi_host_put(host_info->scsi_host);
        sdrv_host_info_free(host_info);

        sdrv_rmv_hba_if_no_host(hba_id);
    }while(0);

    SDRV_HBA_MGR_UNLOCK();

    if(!ret)
    {
        sdrv_info("remove host[%u,%u-%u] succeeded", sys_host_no, hba_id, host_id);
    }
    
    return ret;
}

//bugs with multi conns
int sdrv_conn_host(struct file * file, __u16 hba_id, __u16 host_id)
{
    __u8 need_wake_up = 0;
    struct list_head * node, * next;
    struct sdrv_host_info * host_info;
    __u32 sys_host_no;

    if(unlikely(!file))
    {
        sdrv_err("file %p null", file);
        return -EINVAL;
    }
    
    SDRV_HBA_MGR_LOCK();
    
    host_info = sdrv_get_host_info(hba_id, host_id);
    if(unlikely(!host_info))
    {
        SDRV_HBA_MGR_UNLOCK();
        sdrv_err("host[%u-%u] not found", hba_id, host_id);
        return -ENODEV;
    }

    if(host_info->file)
    {
        sdrv_info("host[%u-%u] connected by other file, kick it out", hba_id, host_id);
    }

    file->private_data = host_info;
    host_info->file = file;
    host_info->ref_cnt++;

    sys_host_no = host_info->scsi_host->host_no;

    SDRV_HBA_MGR_UNLOCK();

    //resend rsp_q cmds
    SDRV_HOST_LOCK(host_info);
    list_for_each_safe(node, next, &host_info->rsp_q)
    {
        list_del_init(node);
        list_add_tail(node, &host_info->req_q);
    }
    if(!list_empty(&host_info->req_q)) need_wake_up = 1;
    SDRV_HOST_UNLOCK(host_info);

    if(need_wake_up) wake_up(&host_info->poll_wait_q);

    sdrv_info("file connected with host[%u,%u-%u]", sys_host_no, hba_id, host_id);

    return 0;
}

void sdrv_disconn_host(struct file * file)
{
    struct sdrv_host_info * host_info;
    __u32 sys_host_no;
    __u16 hba_id, host_id;

    if(unlikely(!file))
    {
        sdrv_err("file %p null", file);
        return;
    }

    host_info = file->private_data;
    if(unlikely(!host_info))
    {
        sdrv_err("file private_data null");
        return;
    }
    
    SDRV_HBA_MGR_LOCK();
    
    sys_host_no = host_info->scsi_host->host_no;
    hba_id = host_info->hba_id;
    host_id = host_info->host_id;
    
    if(host_info->file == file) host_info->file = NULL;
    if(host_info->ref_cnt)
        host_info->ref_cnt--;
    else
        sdrv_err("host[%u,%u-%u] ref_cnt is 0", sys_host_no, hba_id, host_id);
    
    SDRV_HBA_MGR_UNLOCK();

    file->private_data = NULL;

    sdrv_info("file disconnected with host[%u,%u-%u]", sys_host_no, hba_id, host_id);

    return;
}

struct scsi_device * sdrv_get_dev(struct sdrv_host_info * host_info, __u32 channel, __u32 id, __u32 lun)
{
    struct scsi_device * sdev = NULL;

	if(unlikely(!host_info || !host_info->scsi_host)) return NULL;

    list_for_each_entry(sdev, &(host_info->scsi_host->__devices), siblings)
    {
        if(sdev->channel == channel && sdev->id == id && sdev->lun == lun && sdev->sdev_state != SDEV_DEL)
        {
            return sdev;
        }
    }
    
    return NULL;
}

int sdrv_add_dev(struct sdrv_ioctl_dev_info * dev_info)
{
    int ret = 0;
    struct sdrv_host_info * host_info = NULL;
    
    if(unlikely(!dev_info))
    {
        sdrv_err("dev_info null");
        return -EINVAL;
    }

    SDRV_HBA_MGR_LOCK();

    host_info = sdrv_get_host_info(dev_info->hba_id, dev_info->host_id);
    if(unlikely(!host_info))
    {
        SDRV_HBA_MGR_UNLOCK();
        sdrv_err("host[%u-%u] not found", dev_info->hba_id, dev_info->host_id);
        return -ENXIO;
    }
    
    if(unlikely(sdrv_get_dev(host_info, dev_info->channel_id, dev_info->target_id, dev_info->lun_id)))
    {
        sdrv_err("device[%u:%u:%u:%u,%u-%u] already exist", host_info->scsi_host->host_no,
			    dev_info->channel_id, dev_info->target_id, dev_info->lun_id, dev_info->hba_id, dev_info->host_id);
		SDRV_HBA_MGR_UNLOCK();
        return -EEXIST;
    }

    host_info->ref_cnt++;

    SDRV_HBA_MGR_UNLOCK();

    ret = scsi_add_device(host_info->scsi_host, dev_info->channel_id, dev_info->target_id, dev_info->lun_id);
    if(unlikely(ret))
    {
        sdrv_err("add device[%u:%u:%u:%u,%u-%u] failed", host_info->scsi_host->host_no, dev_info->channel_id,
                dev_info->target_id, dev_info->lun_id, dev_info->hba_id, dev_info->host_id);
    }
    else
    {
        sdrv_info("add device[%u:%u:%u:%u,%u-%u] succeeded", host_info->scsi_host->host_no, dev_info->channel_id,
                dev_info->target_id, dev_info->lun_id, dev_info->hba_id, dev_info->host_id);
    }

    SDRV_HBA_MGR_LOCK();
    host_info->ref_cnt--;
    SDRV_HBA_MGR_UNLOCK();

    return ret;
}

void sdrv_scsi_device_put(struct scsi_device * sdev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    scsi_device_put(sdev);
#endif
    return;
}


int sdrv_rmv_dev(struct sdrv_ioctl_dev_info * dev_info)
{
    struct sdrv_host_info * host_info = NULL;
    struct scsi_device * sdev = NULL;
    
    if(unlikely(!dev_info))
    {
        sdrv_err("dev_info null");
        return -EINVAL;
    }

    SDRV_HBA_MGR_LOCK();

    host_info = sdrv_get_host_info(dev_info->hba_id, dev_info->host_id);
    if(unlikely(!host_info))
    {
        SDRV_HBA_MGR_UNLOCK();
        sdrv_info("host[%u-%u] not found", dev_info->hba_id, dev_info->host_id);
        return 0;
    }

    sdev = sdrv_get_dev(host_info, dev_info->channel_id, dev_info->target_id, dev_info->lun_id);
    if(unlikely(!sdev))
    {
        sdrv_info("device[%u:%u:%u:%u,%u-%u] not exist", host_info->scsi_host->host_no, dev_info->channel_id,
                    dev_info->target_id, dev_info->lun_id, dev_info->hba_id, dev_info->host_id);
        SDRV_HBA_MGR_UNLOCK();
        return 0;
    }

    scsi_remove_device(sdev);
    sdrv_scsi_device_put(sdev);

    sdrv_info("remove device[%u:%u:%u:%u,%u-%u] succeeded", host_info->scsi_host->host_no, dev_info->channel_id,
                dev_info->target_id, dev_info->lun_id, dev_info->hba_id, dev_info->host_id);

    SDRV_HBA_MGR_UNLOCK();
    
    return 0;
}

void sdrv_remove_all_dev_and_host(void)
{
    struct list_head * node, * next;
    struct sdrv_hba_info * hba_info;
    struct sdrv_host_info * host_info;
    struct scsi_device * sdev, * sdev_next;
    __u32 i;

    SDRV_HBA_MGR_LOCK();
    list_for_each_safe(node, next, &g_sdrv_hba_mgr.hba_list)
    {
        hba_info = list_entry(node, struct sdrv_hba_info, node);
        
        for(i = 0; i < SDRV_MAX_HOST_PER_HBA; i++)
        {
            if(hba_info->hosts[i])
            {
                BUG_ON(i >= hba_info->host_num);
                
                host_info = hba_info->hosts[i];
                BUG_ON(!host_info->scsi_host);

                //remove devices
                list_for_each_entry_safe(sdev, sdev_next, &(host_info->scsi_host->__devices), siblings)
                {
                    sdrv_info("remove device [%u:%u:%u:%u,%u-%u]",
                        host_info->scsi_host->host_no, sdev->channel, sdev->id, sdev->lun, host_info->hba_id, host_info->host_id);
                    scsi_remove_device(sdev);
                    sdrv_scsi_device_put(sdev);
                    sdev = NULL;
                }
                //remove host
                sdrv_info("remove host[%u,%u-%u]", host_info->scsi_host->host_no, host_info->hba_id, host_info->host_id);
                scsi_remove_host(host_info->scsi_host);
                scsi_host_put(host_info->scsi_host);
                sdrv_host_info_free(host_info);

                hba_info->hosts[i] = NULL;
                host_info = NULL;
            }
        }
        
        hba_info->host_num = 0;
        sdrv_rmv_hba_if_no_host(hba_info->hba_id);
    }
    
    SDRV_HBA_MGR_UNLOCK();

}

inline struct list_head * list_del_tail(struct list_head * head)
{
    struct list_head * entry = head->prev;
    if(entry != head)
    {
        list_del_init(entry);
        return entry;
    }
    else
    {
        return NULL;
    }
}

inline struct list_head * list_del_first(struct list_head * head)
{
    struct list_head * entry = head->next;
    if(entry != head)
    {
        list_del_init(entry);
        return entry;
    }
    else
    {
        return NULL;
    }
}






