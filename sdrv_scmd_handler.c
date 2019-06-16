
#include "sdrv_comm.h"

#define SDRV_VENDOR_ID      "HUST"
#define SDRV_PRODUCT_ID     "SDRV VIRT IO"
#define SDRV_PRODUCT_REV    "1.0"

extern int g_sdrv_null_io;

void copy_data_to_sg(struct scsi_cmnd * sc, char * data, __u32 len, bool user_data)
{
    unsigned int sl_count = scsi_sg_count(sc);
    struct scatterlist * sls = scsi_sglist(sc);
    struct scatterlist * cur_sl;
    int i;
    __u32 left_len = len;
    __u32 copy_len;

    for_each_sg(sls, cur_sl, sl_count, i)
    {
        copy_len = left_len > cur_sl->length ? cur_sl->length : left_len;

        if(likely(user_data))
        {
            if(copy_from_user(sg_virt(cur_sl), data, copy_len))
            {
                sdrv_err("copy data to sg failed");
            }
        }
        else
        {
            memcpy(sg_virt(cur_sl), data, copy_len);
        }
        
        left_len -= copy_len;
        data += copy_len;
        if(!left_len) break;
    }
    
}

void copy_sg_to_data(struct scsi_cmnd * sc, char * data, __u32 len, bool user_data)
{
    unsigned int sl_count = scsi_sg_count(sc);
    struct scatterlist * sls = scsi_sglist(sc);
    struct scatterlist * cur_sl;
    int i;
    __u32 left_len = len;
    __u32 copy_len;

    for_each_sg(sls, cur_sl, sl_count, i)
    {
        copy_len = left_len > cur_sl->length ? cur_sl->length : left_len;

        if(likely(user_data))
        {
            if(copy_to_user(data, sg_virt(cur_sl), copy_len))
            {
                sdrv_err("copy sg to data failed");
            }
        }
        else
        {
            memcpy(data, sg_virt(cur_sl), copy_len);
        }
        
        left_len -= copy_len;
        data += copy_len;
        if(!left_len) break;
    }
    
}




#define	BSWAP_8(x)	((x) & 0xff)
#define	BSWAP_16(x)	((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define	BSWAP_32(x)	((BSWAP_16(x) << 16) | BSWAP_16((x) >> 16))
#define	BSWAP_64(x)	((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))


void sdrv_handle_test_unit_ready(struct scsi_cmnd * cmd)
{
    return;
}

__u8 g_sense_key;
__u8 g_asc;         //additional sense code
__u8 g_ascq;        //additional sense code qualifier

void sdrv_handle_inquiry(struct scsi_cmnd * cmd)
{
    char * data = (char*)sg_virt(cmd->sdb.table.sgl);

    __u8 evpd = cmd->cmnd[1] & 0x01;
    __u8 page_code = cmd->cmnd[2];
    if(evpd || page_code)
    {
        sdrv_info("evpd not support");
        cmd->result = 0x01; //check condition
        //Illegal Request - invalid field in CDB (Command Descriptor Block)
        g_sense_key = 0x05; //
        g_asc = 0x24;
        g_ascq = 0x00;
        return;
    }

    data[2] = 5;    /* SPC-3 */
    data[3] = 0x12;
    data[4] = 31; /* Additional Length */
    //data[5] = 0x08; //3pc not support
    
    data[7] = 0x02;

    memset(data+8, 0x20, 28);

    memcpy(data+8, SDRV_VENDOR_ID, strlen(SDRV_VENDOR_ID)+1);
    memcpy(data+16, SDRV_PRODUCT_ID, strlen(SDRV_PRODUCT_ID)+1);
    memcpy(data+32, SDRV_PRODUCT_REV, strlen(SDRV_PRODUCT_REV)+1);

    return;
}


typedef struct scsi_capacity10 {
    __u32 ret_lba;
    __u32 blk_size;
}scsi_capacity10_t;

typedef struct scsi_capacity16 {
    __u64 ret_lba;
    __u32 blk_size;
}scsi_capacity16_t;


#define SDRV_DISK_SECTORS   (100*2*1024L)   //100M/512B
__u8 g_disk[SDRV_DISK_SECTORS][512];

void sdrv_handle_read_capacity10(struct scsi_cmnd * cmd)
{
    scsi_capacity10_t * cap = sg_virt(cmd->sdb.table.sgl);
    
    cap->ret_lba = BSWAP_32(SDRV_DISK_SECTORS - 1);
    cap->blk_size = BSWAP_32(512);
    
    return;
}

void sdrv_handle_read_capacity16(struct scsi_cmnd * cmd)
{
    scsi_capacity16_t * cap = sg_virt(cmd->sdb.table.sgl);
    
    cap->ret_lba = BSWAP_64(SDRV_DISK_SECTORS - 1);
    cap->blk_size = BSWAP_32(512);
    
    return;
}

void sdrv_handle_mode_sense6(struct scsi_cmnd * cmd)
{
    return;
}


void sdrv_handle_read10(struct scsi_cmnd * cmd)
{
    //sdrv_info("read cmd from %d(%s)", current->pid, current->comm);

    //char * data;
    __u16 xfer_len;
    __u32 lba_sectors = *(__u32*)(&(cmd->cmnd[2]));
    lba_sectors = BSWAP_32(lba_sectors);

    xfer_len = *(__u16*)(&(cmd->cmnd[7]));
    xfer_len = BSWAP_16(xfer_len);

    //sdrv_info("read10: lba %u len %u", lba_sectors, xfer_len);

    if(lba_sectors >= SDRV_DISK_SECTORS)
    {
        sdrv_err("error");
        return;
    }

    if(g_sdrv_null_io != SDRV_NULL_IO_LEVEL_3)
        copy_data_to_sg(cmd, &(g_disk[lba_sectors][0]), xfer_len*512, false);

    //data = (char*)sg_virt(cmd->sdb.table.sgl);
    //memcpy(data, &(g_disk[lba_sectors][0]), xfer_len*512);
    
    return;
}

void sdrv_handle_write10(struct scsi_cmnd * cmd)
{
    //char * data;
    __u16 xfer_len;
    __u32 lba_sectors = *(__u32*)(&(cmd->cmnd[2]));
    lba_sectors = BSWAP_32(lba_sectors);

    xfer_len = *(__u16*)(&(cmd->cmnd[7]));
    xfer_len = BSWAP_16(xfer_len);

    //sdrv_info("write10: lba %u len %u", lba_sectors, xfer_len);

    if(lba_sectors >= SDRV_DISK_SECTORS)
    {
        sdrv_err("error");
        return;
    }

    if(g_sdrv_null_io != SDRV_NULL_IO_LEVEL_3)
        copy_sg_to_data(cmd, &(g_disk[lba_sectors][0]), xfer_len*512, false);
    //data = (char*)sg_virt(cmd->sdb.table.sgl);
    //memcpy(&(g_disk[lba_sectors][0]), data, xfer_len*512);
    
    return;
}



char g_print_buffer[1024];
char g_tmp_buf[128];
void print_cmd(unsigned char * cmd)
{
    int i;
    g_print_buffer[0] = 0;
    
    for(i = 1; i <= 16; i++)
    {
        sprintf(g_tmp_buf, " %02x", cmd[i-1]);
        strcat(g_print_buffer, g_tmp_buf);
        if(i%8 == 0)
        {
            strcat(g_print_buffer, " ");
        }
    }
    
    printk("%s", g_print_buffer);

    return;
}

void sdrv_handle_scsi_cmd(struct scsi_cmnd * cmd, void (*done)(struct scsi_cmnd *))
{
    unsigned char cmd_type = cmd->cmnd[0];

    switch(cmd_type)
    {
    case 0x00:
        print_cmd(cmd->cmnd);
        sdrv_handle_test_unit_ready(cmd);
        break;
    case 0x12:
        print_cmd(cmd->cmnd);
        sdrv_handle_inquiry(cmd);
        break;
    case 0x25:
        print_cmd(cmd->cmnd);
        sdrv_handle_read_capacity10(cmd);
        break;
    case 0x9e:
        print_cmd(cmd->cmnd);
        sdrv_handle_read_capacity16(cmd);
        break;
    case 0x1a:
        print_cmd(cmd->cmnd);
        sdrv_handle_mode_sense6(cmd);
        break;
    case 0x28:
        sdrv_handle_read10(cmd);
        break;
    case 0x2a:
        sdrv_handle_write10(cmd);
        break;
    default:
        sdrv_err("unknown cmd_type %u", cmd_type);
        print_cmd(cmd->cmnd);
        break;
    }
    
    done(cmd);
    
    return;

}


