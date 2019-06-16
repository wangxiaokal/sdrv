/*
for external using
*/

#ifndef __SDRV_H_
#define __SDRV_H_


#if defined(__KERNEL__) || defined(__linux__)

#include <linux/types.h>
#include <asm/ioctl.h>

#else /* One of the BSDs */

#include <sys/ioccom.h>
#include <sys/types.h>


typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;

#endif




#pragma pack (push,1)


#define SDRV_IOCTL_NAME     "sdrv"


/*macros for ioctl*/
#define SDRV_ADD_HOST           (0x100)
#define SDRV_RMV_HOST           (0x101)
#define SDRV_CONN_HOST          (0x102)
//#define SDRV_DISCONN_HOST       (0x103)
#define SDRV_ADD_DEV            (0x200)
#define SDRV_RMV_DEV            (0x201)

#define my_IOWR(x,y,z)          (y)
#define my_IOW(x,y,z)           (y)


#define SDRV_IOCTL_MAGIC        'K'
#define SDRV_IOCTL_ADD_HOST     my_IOWR(SDRV_IOCTL_MAGIC, SDRV_ADD_HOST, 0)
#define SDRV_IOCTL_RMV_HOST     my_IOW(SDRV_IOCTL_MAGIC, SDRV_RMV_HOST, 0)
#define SDRV_IOCTL_CONN_HOST    my_IOWR(SDRV_IOCTL_MAGIC, SDRV_CONN_HOST, 0)
//#define SDRV_IOCTL_DISCONN_HOST my_IOW(SDRV_IOCTL_MAGIC, SDRV_DISCONN_HOST, 0)
#define SDRV_IOCTL_ADD_DEV      my_IOW(SDRV_IOCTL_MAGIC, SDRV_ADD_DEV, 0)
#define SDRV_IOCTL_RMV_DEV      my_IOW(SDRV_IOCTL_MAGIC, SDRV_RMV_DEV, 0)

/*end macros for ioctl*/


struct sdrv_ioctl_host_info
{
    __u16 hba_id;   //vbc's nid in general
    __u16 host_id;  //vbc's thread_id in general

    __u32 max_channel_num;
    __u32 max_target_num;
    __u32 max_lun_num;
    
    __u32 max_cdb_len;
    __u32 max_cmd_num;
    __u32 max_cmds_per_lun;

    __u32 sg_tablesize;

    __u32 sys_host_no;

    __u32 pad;

    __u8 reserved[8];
};

struct sdrv_ioctl_dev_info
{
    __u16 hba_id;
    __u16 host_id;
    
    __u32 channel_id;
    __u32 target_id;
    __u32 lun_id;
    
    __u8 reserved[8];
};

#define MAX_CHANNEL_NUM ((__u32)(-1))
#define MAX_TARGET_NUM  ((__u32)(-1))
#define MAX_LUN_NUM     ((__u32)(-1))

#define SDRV_MAX_IO_DEPTH_PER_HOST (1024)


typedef struct sdrv_msg
{
    __u64 msg_sn;
    __u16 hba_id;
    __u16 host_id;
    __u8  cmd_type;
}sdrv_msg_t;


#pragma pack(pop)

#endif


