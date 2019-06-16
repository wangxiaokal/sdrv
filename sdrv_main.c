#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "sdrv_comm.h"

MODULE_LICENSE("GPL");

/*params start*/
int g_log_level = SDRV_LOG_LEVEL_DBG;
module_param(g_log_level, int, S_IRUSR);

int g_sdrv_null_io = 0;
module_param(g_sdrv_null_io, int, S_IRUSR);
/*params end*/


static int __init sdrv_init(void)
{
    sdrv_info("sdrv init...");

    sdrv_host_dev_init();
    
    sdrv_ioctl_init();
    
    return 0;
}

static void __exit sdrv_exit(void)
{
    sdrv_info("sdrv exit...");

    sdrv_ioctl_exit();

    sdrv_remove_all_dev_and_host();
    
    return;
}

module_init(sdrv_init);
module_exit(sdrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wxk");

