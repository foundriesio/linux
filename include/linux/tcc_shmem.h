#define TCC_SHM_ENABLED (1<<31)

#define TCC_SHM_PORT_REQ (1<<0)
#define TCC_SHM_PORT_DEL_REQ (1<<1)
#define TCC_SHM_RESET_REQ (1<<2)
#define TCC_SHM_RESET_SYNC_REQ (1<<3)
#define TCC_SHM_RESET_DONE (1<<4)
#define TCC_SHM_ENABLED (1<< 31)

#define TCC_SHM_A72_REQ_OFFSET 0x0
#define TCC_SHM_A53_REQ_OFFSET 0x4
#define TCC_SHM_BASE_OFFSET 0x8
#define TCC_SHM_SIZE_OFFSET 0xc
#define TCC_SHM_PORT_OFFSET 0x10
#define TCC_SHM_A72_READ_REQ_OFFSET 0x14
#define TCC_SHM_A53_READ_REQ_OFFSET 0x18
#define TCC_SHM_NAME_OFFSET 0x1c
#define TCC_SHM_NAME_SPACE 0x20
#define TCC_SHM_NAME_SIZE 0x40

#define HEAD_OFFSET_72 (0x0+1)
#define HEAD_OFFSET_53 (0x4+1)
#define TAIL_OFFSET_72 (0x8+1)
#define TAIL_OFFSET_53 (0xc+1)

#define SUCCESS     0

#define tcc_shmem_readl              __raw_readl
#define tcc_shmem_writel             __raw_writel

#define TCC_SHMEM_IOC_MAGIC 's'

#define TCC_SHMEM_CREATE_PORT \
        _IOWR(TCC_SHMEM_IOC_MAGIC, 1, struct tcc_shm_data)

#define SHM_PORT_EXIST      1
#define SHM_PORT_REQUESTED  2

#define TCCSHM_BASE_OFFSET_VAL 0x200

#define SHM_RECV_BUF_NUM 9014 //for jumbo packet

#define SHM_SYNC_TIMEOUT 100

struct tcc_shm_port_list {
    int32_t port_num;
};

struct tcc_shm_mem_map {
    int32_t port_num;
    uint32_t size;
    uint32_t offset;
};

struct tcc_shm_callback {

    int32_t (*callback_func)(unsigned long data, char* received_data, uint32_t received_num);
    unsigned long data;

};

struct tcc_shm_req_port {
	char *name;
	uint32_t size;
	struct list_head list;
};

struct tcc_shm_desc {
    int32_t port_num;
    uint32_t size;
    uint32_t offset_72;
    uint32_t offset_53;
	uint32_t bit_place; //for exported function transfer
	void __iomem *a53_dist_reg; //for exported function transfer
	void __iomem *a72_dist_reg; //for exported function transfer
	uint32_t pending_offset; //for exported function transfer
    uint32_t core_num; //for exported function transfer
    void __iomem *base; //for exported function transfer
    struct list_head list;
    struct tcc_shm_callback shm_callback;
	char *name;
};

struct tcc_shm_data {
    int32_t port_req;
    int32_t port_del_req;
	int32_t reset_req;
	int32_t initial_reset_req;
    struct device *dev;
    void __iomem *base;
    uint32_t core_num;
    uint32_t pending_offset;
    uint32_t reg_phy;
    void __iomem *a53_dist_reg;
    void __iomem *a72_dist_reg;
    uint32_t bit_place;
    uint32_t size;
    uint32_t irq_mode;
    uint32_t read_req;
    int32_t read_que[32];
    uint32_t que_pos;
    uint32_t que_next_pos;
	uint32_t irq;
};

int32_t tcc_shmem_register_callback(int32_t port, struct tcc_shm_callback shm_callback);
int32_t tcc_shmem_transfer_port_nodev(int32_t port, uint32_t size, char *data);
int32_t tcc_shmem_request_port_by_name(char* name, uint32_t size);
int32_t tcc_shmem_find_port_by_name(char* name);
uint32_t tcc_shmem_is_valid(void);
