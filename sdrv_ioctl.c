#include "sdrv_comm.h"

static int g_sdrv_ioctl_majorno = -1;
static struct class * g_sdrv_class;


int sdrv_ioctl_add_host(struct file * file, char __user * p_user)
{
    struct sdrv_ioctl_host_info ah_info = {0};
    long cp_ret;
    int ret;
    
    if(unlikely(!file || !p_user))
    {
        sdrv_err("file %p or p_user %p null", file, p_user);
        return -EINVAL;
    }

    cp_ret = copy_from_user(&ah_info, p_user, sizeof(ah_info));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy from user failed, ret=%ld", cp_ret);
        return -EFAULT;
    }

    ret = sdrv_add_host(&ah_info);
    if(unlikely(ret))
    {
        sdrv_err("add host[%u-%u] failed, ret=%d", ah_info.hba_id, ah_info.host_id, ret);
        return ret;
    }
    else
    {
        cp_ret = copy_to_user(p_user, &ah_info, sizeof(ah_info));
        if(unlikely(cp_ret))
        {
            sdrv_err("copy to user failed, ret=%ld", cp_ret);
            (void)sdrv_rmv_host(ah_info.hba_id, ah_info.host_id);
            return -EFAULT;
        }

        return 0;
    }
}

int sdrv_ioctl_rmv_host(struct file * file, char __user * p_user)
{
    struct sdrv_ioctl_host_info rh_info = {0};
    int ret;
    long cp_ret;
    
    if(unlikely(!file || !p_user))
    {
        sdrv_err("file %p or p_user %p null", file, p_user);
        return -EINVAL;
    }

    cp_ret = copy_from_user(&rh_info, p_user, sizeof(struct sdrv_ioctl_host_info));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy from user failed, ret=%ld", cp_ret);
        return -EFAULT;
    }

    ret = sdrv_rmv_host(rh_info.hba_id, rh_info.host_id);
    if(unlikely(ret))
    {
        sdrv_err("remove host[%u-%u] failed", rh_info.hba_id, rh_info.host_id);
    }

    return ret;
}

int sdrv_ioctl_conn_host(struct file * file, char __user * p_user)
{
    struct sdrv_ioctl_host_info ch_info = {0};
    int ret;
    long cp_ret;
    
    if(unlikely(!file || !p_user))
    {
        sdrv_err("file %p or p_user %p null", file, p_user);
        return -EINVAL;
    }

    cp_ret = copy_from_user(&ch_info, p_user, sizeof(struct sdrv_ioctl_host_info));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy from user failed, ret=%ld", cp_ret);
        return -EFAULT;
    }

    ret = sdrv_conn_host(file, ch_info.hba_id, ch_info.host_id);
    if(unlikely(ret))
    {
        sdrv_err("connect host[%u-%u] failed", ch_info.hba_id, ch_info.host_id);
    }

    return ret;
}

int sdrv_ioctl_add_dev(struct file * file, char __user * p_user)
{
    struct sdrv_ioctl_dev_info dev_info;
    int ret;
    long cp_ret;
    
    if(unlikely(!file || !p_user))
    {
        sdrv_err("file %p or p_user %p null", file, p_user);
        return -EINVAL;
    }

    cp_ret = copy_from_user(&dev_info, p_user, sizeof(dev_info));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy from user failed, ret=%ld", cp_ret);
        return -EFAULT;
    }

    ret = sdrv_add_dev(&dev_info);
    if(unlikely(ret))
    {
        sdrv_err("add dev[?:%u:%u:%u,%u-%u] failed", dev_info.channel_id, dev_info.target_id, dev_info.lun_id, dev_info.hba_id, dev_info.host_id);
    }

    return ret;
}

int sdrv_ioctl_rmv_dev(struct file * file, char __user * p_user)
{
    struct sdrv_ioctl_dev_info dev_info;
    int ret;
    long cp_ret;
    
    if(unlikely(!file || !p_user))
    {
        sdrv_err("file %p or p_user %p null", file, p_user);
        return -EINVAL;
    }

    cp_ret = copy_from_user(&dev_info, p_user, sizeof(dev_info));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy from user failed, ret=%ld", cp_ret);
        return -EFAULT;
    }

    ret = sdrv_rmv_dev(&dev_info);
    if(unlikely(ret))
    {
        sdrv_err("remove dev[?:%u:%u:%u,%u-%u] failed", dev_info.channel_id, dev_info.target_id, dev_info.lun_id, dev_info.hba_id, dev_info.host_id);
    }

    return ret;
}

static long sdrv_ioctl_dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    char __user * p_user = (char __user *)arg;

    sdrv_info("recv IOCTL cmd %#x from %s(pid=%d), arg=%lu", cmd, current->comm, current->pid, arg);
    
    switch(cmd)
    {
    case SDRV_IOCTL_ADD_HOST:
        ret = sdrv_ioctl_add_host(file, p_user);
        break;
    case SDRV_IOCTL_RMV_HOST:
        ret = sdrv_ioctl_rmv_host(file, p_user);
        break;
    case SDRV_IOCTL_CONN_HOST:
        ret = sdrv_ioctl_conn_host(file, p_user);
        break;
    case SDRV_IOCTL_ADD_DEV:
        ret = sdrv_ioctl_add_dev(file, p_user);
        break;
    case SDRV_IOCTL_RMV_DEV:
        ret = sdrv_ioctl_rmv_dev(file, p_user);
        break;
    default:
        sdrv_info("unknown IOCTL cmd %#x from %s(pid=%d), arg=%lu", cmd, current->comm, current->pid, arg);
        ret = -EINVAL;
        break;
    }
    
    return ret;
}

static long sdrv_ioctl_dev_compat_ioctl(struct file * file, unsigned int cmd, unsigned long arg)
{
    return sdrv_ioctl_dev_ioctl(file, cmd, arg);
}

static unsigned int sdrv_ioctl_dev_poll(struct file * file, struct poll_table_struct * table)
{
    struct sdrv_host_info * host_info = file->private_data;
    unsigned int mask = 0;

    if(unlikely(!host_info))
    {
        sdrv_err("file private_data is null");
        return -ENODEV;
    }

    if(host_info->file != file)
    {
        sdrv_err("host file %p not equal with file %p", host_info->file, file);
        return -EACCES;
    }
    
    poll_wait(file, &host_info->poll_wait_q, table);    //place in the locked area?

    SDRV_HOST_LOCK(host_info);
    if(!list_empty(&host_info->req_q))
    {
        mask |= POLLIN | POLLRDNORM;
    }
    SDRV_HOST_UNLOCK(host_info);

    //sdrv_info("poll from %s(pid=%d)", current->comm, current->pid);

    return mask;
}


static int sdrv_ioctl_dev_open(struct inode * inode, struct file * file)
{
    return 0;
}


static int sdrv_ioctl_dev_release(struct inode * inode, struct file * file)
{
    if(file->private_data)
    {
        sdrv_disconn_host(file);
    }
    return 0;
}

ssize_t sdrv_ioctl_dev_read(struct file * file, char __user * p_user, size_t size, loff_t * offset)
{
    struct sdrv_msg msg = {0};
    struct list_head * node;
    struct sdrv_scsi_cmd_req * scmd_req;
    struct sdrv_host_info * host_info = file->private_data;
    long cp_ret;
    if(unlikely(!host_info)) return 0;

    if(unlikely(host_info->file != file))
    {
        sdrv_err("host_info file %p not equal with file %p", host_info->file, file);
        return -EACCES;
    }

    SDRV_HOST_LOCK(host_info);
    node = list_del_first(&host_info->req_q);
    if(unlikely(!node))
    {
        SDRV_HOST_UNLOCK(host_info);
        sdrv_err("req q null");
        return 0;
    }

    scmd_req = list_entry(node, struct sdrv_scsi_cmd_req, node);

    list_add_tail(&scmd_req->node, &host_info->rsp_q);
    SDRV_HOST_UNLOCK(host_info);

    msg.hba_id = host_info->hba_id;
    msg.host_id = host_info->host_id;
    msg.msg_sn = scmd_req->cmd_sn;
    msg.cmd_type = scmd_req->cmd->cmnd[0];
    //memcpy(&msg.scmd, scmd_req->cmd, sizeof(msg.scmd));

    cp_ret = copy_to_user(p_user, &msg, sizeof(msg));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy filed");
        return 0;
    }

    return sizeof(msg);
}

ssize_t sdrv_ioctl_dev_write(struct file * file, const char __user * p_user, size_t size, loff_t * offset)
{
    struct scsi_cmnd * cmd = NULL;
    void (*done)(struct scsi_cmnd *) = NULL;
    struct sdrv_msg msg = {0};
    struct sdrv_scsi_cmd_req * scmd_req;
    struct sdrv_host_info * host_info = file->private_data;
    struct list_head * node;
    int cp_ret;
    if(unlikely(!host_info)) return 0;

    if(unlikely(host_info->file != file))
    {
        sdrv_err("host_info file %p not equal with file %p", host_info->file, file);
        return -EACCES;
    }

    cp_ret = copy_from_user(&msg, p_user, sizeof(msg));
    if(unlikely(cp_ret))
    {
        sdrv_err("copy from user failed");
        return 0;
    }

    if(unlikely(msg.hba_id != host_info->hba_id || msg.host_id != host_info->host_id))
    {
        sdrv_err("msg host[%u-%u] not equal with file host [%u-%u]", msg.hba_id, msg.host_id, host_info->hba_id, host_info->host_id);
        return 0;
    }

    SDRV_HOST_LOCK(host_info);
    list_for_each(node, &host_info->rsp_q)
    {
        scmd_req = list_entry(node, struct sdrv_scsi_cmd_req, node);
        if(scmd_req->cmd_sn == msg.msg_sn)
        {
            list_del_init(&scmd_req->node);
            cmd = scmd_req->cmd;
            done = scmd_req->done;
            host_scmd_req_free(host_info, scmd_req);
            break;
        }
    }
    SDRV_HOST_UNLOCK(host_info);

    if(cmd && done)
    {
        sdrv_handle_scsi_cmd(cmd, done);
        return sizeof(msg);
    }
    else
    {
        sdrv_err("nobody waiting for this msg, msg_sn=%llu", msg.msg_sn);
        return 0;
    }
}


static const struct file_operations g_sdrv_ioctl_fops =
{
    .owner             = THIS_MODULE,
    .open              = sdrv_ioctl_dev_open,
    //.flush             = sdrv_ioctl_dev_f_flush,
    .release           = sdrv_ioctl_dev_release,
    .read              = sdrv_ioctl_dev_read,
    .write             = sdrv_ioctl_dev_write,
    .poll              = sdrv_ioctl_dev_poll,
    .unlocked_ioctl    = sdrv_ioctl_dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl      = sdrv_ioctl_dev_compat_ioctl,
#endif
};


int sdrv_ioctl_init(void)
{
    int majorno = -1;
    int ret     = -1;
    struct device * sdrv_dev = NULL;
    
    majorno = register_chrdev(0, SDRV_IOCTL_NAME, &g_sdrv_ioctl_fops);
    if(unlikely(majorno < 0))
    {
        sdrv_err("register_chrdev %s failed, majorno:%d", SDRV_IOCTL_NAME, majorno);
        ret = majorno;
        goto err_ret;
    }

    g_sdrv_ioctl_majorno = majorno;

    g_sdrv_class = class_create(THIS_MODULE, SDRV_IOCTL_NAME);
    if(unlikely(IS_ERR(g_sdrv_class)))
    {
        sdrv_err("class_create[%p] failed", g_sdrv_class);
        ret = PTR_ERR(g_sdrv_class);
        goto err_ret;
    }
    
    sdrv_dev = device_create(g_sdrv_class, NULL, MKDEV(g_sdrv_ioctl_majorno, 0), NULL, SDRV_IOCTL_NAME);

    if(unlikely(IS_ERR(sdrv_dev)))
    {
        sdrv_err("device_create[%p] failed", sdrv_dev);
        ret = PTR_ERR(sdrv_dev);
        goto err_ret;
    }

    return 0;
    
err_ret:
    if(!IS_ERR(sdrv_dev))
    {
        device_destroy(g_sdrv_class, MKDEV(g_sdrv_ioctl_majorno, 0));
    }
    
    if(!IS_ERR(g_sdrv_class))
    {
        class_destroy(g_sdrv_class);
        g_sdrv_class = NULL;
    }
    
    if(g_sdrv_ioctl_majorno >= 0)
    {
       unregister_chrdev(g_sdrv_ioctl_majorno, SDRV_IOCTL_NAME); 
    }
    
    return ret;
}

void sdrv_ioctl_exit(void)
{
    device_destroy(g_sdrv_class, MKDEV(g_sdrv_ioctl_majorno, 0));
    
    class_destroy(g_sdrv_class);
    g_sdrv_class = NULL;
    
    unregister_chrdev(g_sdrv_ioctl_majorno, SDRV_IOCTL_NAME); 
    
    return;
}

