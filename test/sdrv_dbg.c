#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#include "../sdrv.h"

#define DEV_PATH "/dev/"SDRV_IOCTL_NAME

typedef int (*ioctl_request_fn)(int argc, char * argv[]);

typedef struct hdl_fn_tbl
{
    char * cmd_str;
    ioctl_request_fn hld_fn;
    char * param_desc;
}hdl_fn_tbl_t;

void print_usage();


int add_host(int argc, char * argv[])
{
    struct sdrv_ioctl_host_info add_host_info = {0};
    
    add_host_info.max_channel_num = MAX_CHANNEL_NUM;
    add_host_info.max_target_num = MAX_TARGET_NUM;
    add_host_info.max_lun_num = MAX_LUN_NUM;
    
    add_host_info.max_cdb_len = 2048;
    add_host_info.max_cmd_num = 1;
    add_host_info.max_cmds_per_lun = 1;
    
    add_host_info.sg_tablesize = 128;

    if(argc < 4 || argc > 11)
    {
        printf("argc %d < 4 or > 11\n", argc);
        print_usage();
        return -1;
    }

    add_host_info.hba_id = atoi(argv[2]);
    add_host_info.host_id = atoi(argv[3]);

    
    if(argc >= 5) add_host_info.max_channel_num = atoi(argv[4]);
    if(argc >= 6) add_host_info.max_target_num = atoi(argv[5]);
    if(argc >= 7) add_host_info.max_lun_num = atoi(argv[6]);
    
    if(argc >= 8) add_host_info.max_cdb_len = atoi(argv[7]);
    if(argc >= 9) add_host_info.max_cmd_num = atoi(argv[8]);
    if(argc >= 10) add_host_info.max_cmds_per_lun = atoi(argv[9]);
    
    if(argc >= 11) add_host_info.sg_tablesize = atoi(argv[10]);


    int fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    int ret = ioctl(fd, SDRV_IOCTL_ADD_HOST, &add_host_info);
    if(ret)
    {
        printf("add host failed, ret = %d\n", ret);
    }
    else
    {
        printf("add host[%u-%u] succeed, sys_host_no=%u\n", add_host_info.hba_id, add_host_info.host_id, add_host_info.sys_host_no);
    }

    close(fd);
    return ret;
}

int rmv_host(int argc, char * argv[])
{
    if(argc != 4)
    {
        printf("argc %d != 4\n", argc);
        print_usage();
        return -1;
    }

    struct sdrv_ioctl_host_info rh_info = {0};
    rh_info.hba_id = atoi(argv[2]);
    rh_info.host_id = atoi(argv[3]);

    int fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    int ret = ioctl(fd, SDRV_IOCTL_RMV_HOST, &rh_info);
    if(ret)
    {
        printf("rmv host failed, ret = %d\n", ret);
    }
    else
    {
        printf("rmv host[%u-%u] succeed\n", rh_info.hba_id, rh_info.host_id);
    }

    close(fd);
    return 0;
}

int conn_host(int argc, char * argv[])
{
    if(argc != 4)
    {
        printf("argc %d != 4\n", argc);
        print_usage();
        return -1;
    }

    struct sdrv_ioctl_host_info ch_info = {0};
    ch_info.hba_id = atoi(argv[2]);
    ch_info.host_id = atoi(argv[3]);

    int fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    int ret = ioctl(fd, SDRV_IOCTL_CONN_HOST, &ch_info);
    if(ret)
    {
        printf("conn host failed, ret = %d\n", ret);
    }
    else
    {
        printf("conn host[%u-%u] succeed, =%d\n", ch_info.hba_id, ch_info.host_id);
    }

    close(fd);
    return 0;
}


int add_dev(int argc, char * argv[])
{
    if(argc != 7)
    {
        printf("argc %d != 7\n", argc);
        print_usage();
        return -1;
    }

    struct sdrv_ioctl_dev_info dev_info = {0};
    dev_info.hba_id = atoi(argv[2]);
    dev_info.host_id = atoi(argv[3]);
    dev_info.channel_id = atoi(argv[4]);
    dev_info.target_id = atoi(argv[5]);
    dev_info.lun_id = atoi(argv[6]);

    int fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    int ret = ioctl(fd, SDRV_IOCTL_ADD_DEV, &dev_info);
    if(ret)
    {
        printf("add dev failed, ret = %d\n", ret);
    }
    else
    {
        printf("add dev succeed\n");
    }

    close(fd);
    return 0;
}

int rmv_dev(int argc, char * argv[])
{
    if(argc != 7)
    {
        printf("argc %d != 6\n", argc);
        print_usage();
        return -1;
    }

    struct sdrv_ioctl_dev_info dev_info = {0};
    dev_info.hba_id = atoi(argv[2]);
    dev_info.host_id = atoi(argv[3]);
    dev_info.channel_id = atoi(argv[4]);
    dev_info.target_id = atoi(argv[5]);
    dev_info.lun_id = atoi(argv[6]);

    int fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    int ret = ioctl(fd, SDRV_IOCTL_RMV_DEV, &dev_info);
    if(ret)
    {
        printf("rmv dev failed, ret = %d\n", ret);
    }
    else
    {
        printf("rmv dev succeed\n");
    }

    close(fd);
    return 0;
}

unsigned long long g_cmd_cnt = 0;

int handle_io(int argc, char * argv[])
{
    int fd;
    fd_set rds;
    int ret;
    struct sdrv_msg msg = {0};
    
    if(argc != 4 && argc != 5)
    {
        printf("argc %d != 4 && argc != 5\n", argc);
        print_usage();
        return -1;
    }

    struct sdrv_ioctl_host_info hdl_io_info = {0};
    hdl_io_info.hba_id = atoi(argv[2]);
    hdl_io_info.host_id = atoi(argv[3]);

    int show_io = 0;
    if(argc == 5) show_io = atoi(argv[4]);

    fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    ret = ioctl(fd, SDRV_IOCTL_CONN_HOST, &hdl_io_info);
    if(ret)
    {
        printf("conn host failed, ret = %d\n", ret);
        close(fd);
        return -3;
    }
    else
    {
        printf("conn host[%u-%u] succeed\n", hdl_io_info.hba_id, hdl_io_info.host_id);
    }

    FD_ZERO(&rds);
    FD_SET(fd, &rds);

slct:
    ret = select(fd + 1, &rds, NULL, NULL, NULL);
    if (ret < 0) 
    {
        printf("select error!\n");
        close(fd);
        return -4;
    }
    if (FD_ISSET(fd, &rds))
    {
        ret = read(fd, &msg, sizeof(msg));
        if(ret != sizeof(msg))
        {
            printf("read fail, ret=%d, should be %d\n", ret, sizeof(msg));
            if(ret < 0)
            {
                printf("rw_error ret: %d, cmd_cnt: %llu\n", ret, g_cmd_cnt);
                close(fd);
                return -5;
            }
        }
    }
    
    /*检测结果*/
    if(show_io)
        printf("cmd_sn=%#llx, hba_id=%u, host_id=%u, cmd_type=%#x\n", msg.msg_sn, msg.hba_id, msg.host_id, msg.cmd_type);

    ret = write(fd, &msg, sizeof(msg));
    if(ret != sizeof(msg))
    {
        printf("write failed, ret = %d, should be %d\n", ret, sizeof(msg));
        if(ret < 0)
        {
            printf("rw_error ret: %d, cmd_cnt: %llu\n", ret, g_cmd_cnt);
            close(fd);
            return -6;
        }
    }

    g_cmd_cnt++;

    goto slct;
    
    close(fd);

    return 0;
}

int epoll_handle_io(int argc, char * argv[])
{
    int fd;
    fd_set rds;
    int ret;
    struct sdrv_msg msg = {0};
    
    if(argc != 4 && argc != 5)
    {
        printf("argc %d != 4 && argc != 5\n", argc);
        print_usage();
        return -1;
    }

    struct sdrv_ioctl_host_info hdl_io_info = {0};
    hdl_io_info.hba_id = atoi(argv[2]);
    hdl_io_info.host_id = atoi(argv[3]);

    int show_io = 0;
    if(argc == 5) show_io = atoi(argv[4]);

    fd = open(DEV_PATH, O_RDWR, 0);
    if(fd < 0)
    {
        printf("open %s failed, ret = %d\n", DEV_PATH, fd);
        return -2;
    }

    ret = ioctl(fd, SDRV_IOCTL_CONN_HOST, &hdl_io_info);
    if(ret)
    {
        printf("conn host failed, ret = %d\n", ret);
        close(fd);
        return -3;
    }
    else
    {
        printf("conn host[%u-%u] succeed\n", hdl_io_info.hba_id, hdl_io_info.host_id);
    }

    FD_ZERO(&rds);
    FD_SET(fd, &rds);

epoll:
    ret = select(fd + 1, &rds, NULL, NULL, NULL);
    if (ret < 0) 
    {
        printf("select error!\n");
        close(fd);
        return -4;
    }
    if (FD_ISSET(fd, &rds))
    {
        ret = read(fd, &msg, sizeof(msg));
        if(ret != sizeof(msg))
        {
            printf("read fail, ret=%d, should be %d\n", ret, sizeof(msg));
            if(ret < 0)
            {
                printf("rw_error ret: %d, cmd_cnt: %llu\n", ret, g_cmd_cnt);
                close(fd);
                return -5;
            }
        }
    }
    
    /*检测结果*/
    if(show_io)
        printf("cmd_sn=%#llx, hba_id=%u, host_id=%u, cmd_type=%#x\n", msg.msg_sn, msg.hba_id, msg.host_id, msg.cmd_type);

    ret = write(fd, &msg, sizeof(msg));
    if(ret != sizeof(msg))
    {
        printf("write failed, ret = %d, should be %d\n", ret, sizeof(msg));
        if(ret < 0)
        {
            printf("rw_error ret: %d, cmd_cnt: %llu\n", ret, g_cmd_cnt);
            close(fd);
            return -6;
        }
    }

    g_cmd_cnt++;

    goto epoll;
    
    close(fd);

    return 0;
}


hdl_fn_tbl_t g_hdl_tbl[] = {
    {"addhost", add_host, "hba_id host_id [max_channel_num] [max_target_num] [max_lun_num] [max_cdb_len] [max_cmd_num] [max_cmds_per_lun] [sg_tablesize]"},
    {"rmvhost", rmv_host, "hba_id host_id"},
    {"connhost", conn_host, "hba_id host_id"},
    {"adddev", add_dev, "hba_id host_id channel_id target_id lun_id"},
    {"rmvdev", rmv_dev, "hba_id host_id channel_id target_id lun_id"},
    {"handleio", handle_io, "hba_id host_id [show_io]"},
    {"hdl_fn_tbl_nr", NULL, ""}
};

void handle_signal(int signal)
{
    printf("cmd cnt handled: %llu\n", g_cmd_cnt);
    return;
}

void print_usage()
{
    printf("Usage:\n");
    
    int i = 0;
    for(; i < sizeof(g_hdl_tbl)/sizeof(g_hdl_tbl[0]); i++)
    {
        if(g_hdl_tbl[i].hld_fn)
            printf("    %s %s\n", g_hdl_tbl[i].cmd_str, g_hdl_tbl[i].param_desc);
    }
}

int main(int argc, char * argv[])
{
    if(argc < 2)
    {
        printf("argc = %d < 2, error\n", argc);
        print_usage();
        return -1;
    }

    signal(SIGINT, handle_signal);
    
    char * cmd = argv[1];
    assert(cmd);

    int i = 0;
    for(; i < sizeof(g_hdl_tbl)/sizeof(g_hdl_tbl[0]); i++)
    {
        if(strcmp(cmd, g_hdl_tbl[i].cmd_str) == 0 && g_hdl_tbl[i].hld_fn)
        {
            return g_hdl_tbl[i].hld_fn(argc, argv);
        }
    }

    printf("unknown cmd %s\n", cmd);
    print_usage();
    
    return -1;
}
