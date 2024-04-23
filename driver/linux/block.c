#include <linux/major.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/mutex.h>

#define SIMP_BLKDEV_BYTES            (64 * 1024 * 1024)
static char simp_blkdev_data[SIMP_BLKDEV_BYTES];

static DEFINE_SPINLOCK(blockLock);

static int BLOCK_MAJORY = 0;
static char *devName = "PCIeSSD";


static blk_status_t block_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd){
    struct request *req = bd->rq;
	unsigned long start;
	unsigned long len;
    void *buffer = NULL;
    blk_status_t err;
    blk_mq_start_request(req);
    
    spin_lock_irq(&blockLock);
    
    do{
        buffer = bio_data(req->bio);
        len = blk_rq_cur_bytes(req);
        start = blk_rq_pos(req) << SECTOR_SHIFT;
        
        if (rq_data_dir(req) == READ){
            memcpy(buffer, simp_blkdev_data + start, len);
        }else{
            memcpy(simp_blkdev_data + start, buffer, len);
        }
        err = BLK_STS_OK;
    }while(blk_update_request(req, err, blk_rq_cur_bytes(req)));
    
    spin_unlock_irq(&blockLock);
    blk_mq_end_request(req, err);

    return BLK_STS_OK;
}

static struct blk_mq_tag_set tag_set;

static const struct blk_mq_ops PCIeSSD_mq_ops = {
	.queue_rq = block_queue_rq,
};

static const struct block_device_operations PCIeSSD_fops = {
	.owner = THIS_MODULE,
};

static struct gendisk *PCIeSSD_gendisk;

static int USBSSD_register_disk(void){
    // struct request_queue *q;
	struct gendisk *disk;
    // disk = alloc_disk(1);
    int err;
    disk = blk_mq_alloc_disk(&tag_set, NULL);
	if (IS_ERR(disk)){
        return PTR_ERR(disk);
    }
    printk(KERN_EMERG "blk_mq_alloc_disk %p\n", disk);
    // q = blk_mq_init_queue(&tag_set);
    // if (IS_ERR(q)) {
	// 	put_disk(disk);
    //     PCIeSSD_gendisk = NULL;
	// 	return PTR_ERR(q);
	// }
    disk->major = BLOCK_MAJORY;
	disk->first_minor = 0;
    disk->minors = 1;
	disk->fops = &PCIeSSD_fops;
    
	sprintf(disk->disk_name, "USBSSD");
    // disk->queue = q;
    PCIeSSD_gendisk = disk;

    set_capacity(PCIeSSD_gendisk, SIMP_BLKDEV_BYTES >> 9); // number of sector
    err = add_disk(disk);
    printk(KERN_EMERG "blk_mq_alloc_disk %d\n", err);
	if (err){
        PCIeSSD_gendisk = NULL;
		put_disk(disk);
    }

    return err;
}


int init_pcie_block_dev(void){
    int ret = 0;

    BLOCK_MAJORY = register_blkdev(0, devName);

    tag_set.ops = &PCIeSSD_mq_ops;
    tag_set.nr_hw_queues = 1;
	tag_set.queue_depth = 1;
    tag_set.numa_node = NUMA_NO_NODE;
    tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    ret = blk_mq_alloc_tag_set(&tag_set);
    printk(KERN_EMERG "blk_mq_alloc_tag_set\n");
    if (ret)
		goto err1;
    
    ret = USBSSD_register_disk();
    printk(KERN_EMERG "init end %d\n", ret);
    if (ret)
		goto err2;

    return 0;
err2:
    blk_mq_free_tag_set(&tag_set); 
err1:
    unregister_blkdev(BLOCK_MAJORY, devName);
    return ret;
}

void exit_pcie_block_dev(void){
    unregister_blkdev(BLOCK_MAJORY, devName);
    del_gendisk(PCIeSSD_gendisk);
    // blk_cleanup_queue(PCIeSSD_gendisk->queue);
    put_disk(PCIeSSD_gendisk);
    blk_mq_free_tag_set(&tag_set);
}