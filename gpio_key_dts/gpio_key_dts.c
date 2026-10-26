// Linux驱动学习的第二个小demo
/* 现阶段所学的Linux驱动有两种形式
   分别为Platform框架与传统写法
   本代码目前为传统写法
   与第一版不同的地方在于该版本引用了dts
   在.dts文件中对gpio18进行申请
   在该函数中对其进行调用
*/
#include <linux/module.h>       /* MODULE_LICENSE / module_init / module_exit
                                   —— 所有模块驱动的必备头文件 */
#include <linux/kernel.h>       /* printk / 内核核心设施 */
#include <linux/init.h>         /* __init / __exit 宏
                                   —— 标记函数仅在初始化/卸载时使用，
                                   __init 标记的函数执行完后内存被回收省 RAM */
#include <linux/fs.h>           /* file_operations / dev_t / inode 相关
                                   —— 字符设备的注册框架 */
#include <linux/cdev.h>         /* struct cdev / cdev_init / cdev_add
                                   —— 字符设备核心结构 */
#include <linux/device.h>       /* class_create / device_create
                                   —— 自动生成 /dev 节点靠它 */
#include <linux/uaccess.h>      /* copy_to_user
                                   —— 内核态→用户态数据搬运（新内核路径；
                                   老内核用 <asm/uaccess.h>）*/
#include <linux/gpio.h>         /* gpio_request / gpio_direction_input /
                                   gpio_get_value / gpio_free —— 旧版 GPIO API */
#include <linux/of.h>           /* of_find_node_by_name / struct device_node
                                   —— "of" = OpenFirmware，设备树操作函数都在 of_ 前缀下 */
#include <linux/of_gpio.h>      /* of_get_named_gpio —— 从设备树节点提取 GPIO 编号 */
#include <linux/atomic.h>       /* atomic_t / atomic_set / atomic_read
                                   —— 原子变量，为将来中断版预留 */
#include <linux/errno.h>        /* -EINVAL / -EFAULT / -EBUSY 等错误码定义 */





// =======宏定义阶段=======
// 字符设备层：管理几个实例
#define KEY_CNT    1
// 字符设备层：/dev的名字
#define KEY_NAME   "mykey"        /* 须与 dts 节点名一致 */
// 应用契约层，默认设置
#define KEY0VALUE  0xF0
// 应用契约层，默认设置
#define INVAKEY    0x00





// =======定义设备结构体=======
struct key_dev{
    // dev_t本质是u32，其所定义的devid是用来存贮主设备号与次设备号
    dev_t devid;
    // 定义字符设备的核心，其中的cdev是内核提供的结构
    // 该结构里有一个关键的指针，指向file_operations
    struct cdev cdev;
    // 内核的设备类结构体指针，其作用是将同类设备进行归组
    struct class *class;
    // 定义class下所挂载的设备对象
    struct device *device;
    // 主设备号
    int major;
    // 次设备号
    int minor;
    // gpio全局编号
    int key_gpio;
    // 原子变量
    atomic_t key_value;

};
// 实例化结构体
static struct key_dev keydev;





// =======驱动五大块第33333块=======
/* 该模块的作用是进行相关的硬件资源获取（传统写法的半个probe）
   其主要作用是将"dts中的一行文本描述"变为"内核中一个可以操作的、被本驱动独占的GPIO"
   放在module_init而不是open
   GPIO是独占资源，归属驱动本身，open可以发生多次，但init只能发生一次
*/
static int keyio_init(void)
{   
    int ret;

    /*
    之所以不使用结构体中已经定义的device_node，
    是因为只想让这个新定义的device_node只在这个keyio_init函数中存活，
    一旦函数结束，不占用其他空间
    */ 
    struct device_node *nd;
    /*
    在内核已经解析好的 device_node 树里，按节点名查找，找到名字叫 mykey 的节点。
    查找方式：只在同一层的兄弟链上横向遍历（内部 for 循环只走 sibling），
    不做递归下钻——mykey 恰好挂在根层级，所以能命中；若挂在更深层则找不到。
    NULL（第一个参数）：搜索起点。传 NULL = 从根节点所在的这一层开始找。
    "mykey"（第二个参数）：要找的节点名字符串。注意——找的是节点名，不是 compatible。
    找到则返回该节点的地址并存入nd；没找到返回NULL。
    该函数内部会调用 of_node_get()，将节点的引用计数+1（相当于签到）：
    计数器记录"还有多少持有者在用这个节点"，
    计数大于0时节点不允许被内核释放，防止有人还攥着指针时内存被回收（use-after-free防线）。
    计数归0才允许释放节点——所以用完必须 of_node_put() 签退。
    */
    nd = of_find_node_by_name(NULL, "mykey");
    if(!nd){
        return -ENODEV;
    }
    /*
    上一段代码已经将mykey这个节点的地址放入了nd中
    这段代码的含义即为按照nd的地址寻找gpio的相关属性
    最后将预期的gpio编号放入ret中
    */
    ret = of_get_named_gpio(nd, "gpios", 0);
    // 这行代码代表将节点的引用计数-1（签退，与查找时的+1配对）
    of_node_put(nd);
    // 返回错误码
    if(ret < 0){
        return ret;
    }
    // 将读取的gpio号放入结构体指针预留的gpio号中
    keydev.key_gpio = ret;

    /* 申请独占 GPIO：成功独占时返回0；
    未成功独占时返回 -EBUSY */
    ret = gpio_request(keydev.key_gpio, "key0");
    /*
    申请失败返回错误码，函数退出，申请成功则继续运行函数
    */
    if (ret) {
        printk(KERN_ERR "gpio %d request failed: %d\n", keydev.key_gpio, ret);
        return ret;
    }

    /* 配置输入方向：失败则回滚本次申请 */
    /*
    gpio_direction_input()函数的作用是将指定的GPIO配置为输入模式
    输入模式指的是外部电路设备向芯片传递电平信号
    其返回值为0表示成功，非0表示失败
    */
    ret = gpio_direction_input(keydev.key_gpio);
    // 调节GPIO模式成功则继续进行，失败对该GPIO进行释放，回滚本次申请
    if (ret) {
        printk(KERN_ERR "gpio %d set input failed: %d\n", keydev.key_gpio, ret);
        gpio_free(keydev.key_gpio);        /* 申请了就要放——回滚纪律 */
        return ret;
    }
    // 打印信息，说明GPIO申请成功，且配置为输入模式
    printk(KERN_INFO "key_gpio=%d requested, input mode\n", keydev.key_gpio);
    return 0;
}






// =======驱动五大块第44444块=======
// 应用层
// 对应理论中的file_operations结构
/*
先讲解一下inode与dentry的关系
对于/dev/mykey这个路径：
dentry（目录项）= "名字 → inode" 的映射结构，d_name 是它里面的名字字段；
inode = 设备节点的持久元数据（权限、设备号、指向cdev的指针都存在这里）。
open 的本质：按路径逐级查找 dentry → 落到 inode → 取出本驱动注册的 cdev
→ 绑定到本次会话的 filp 上。
struct inode *inode 代表持久存在的设备节点（每次open都是同一个）；
filp 代表本次会话，临时建立（每open一次新建一个）。
本程序没有使用inode中的信息，因为是单实例——直接用全局keydev即可；
多实例驱动才需要从inode里取出次设备号来区分不同设备。
*/
static int key_open(struct inode *inode, struct file *filp)
{
    // 将结构体指针的地址放入(*filp).private_data
    filp->private_data = &keydev;    /* 为多实例扩展预留的接口 */
    return 0;
}
// key_read这个函数于key_open这个函数是绑定的关系
/*
有关输入参数的相关定义
struct file *filp 这个*filp中存储着结构体指针的地址
char __user *buf用户空间的缓冲区地址
size_t cnt表示用户想读多少字节，读的字节数不恩你个超过cnt，否则会发生越界问题
loff_t *offt文件偏移指针，对普通文件，read 后要把 *offt += n 更新偏移
*/
static ssize_t key_read(struct file *filp, char __user *buf,
                        size_t cnt, loff_t *offt)
{
    /*
    将(*filp).private_data中的地址
    放入定义的结构体设备地址
    */ 
    struct key_dev *dev = filp->private_data;
    // 定义一字节的存储空间，用来暂存按键值
    unsigned char value;
    // 轮询
    /*
    (*dev).key_gpio保存的是gpio引脚的全局编号
    通过分析硬件结构得知：按键断开 ==》为高点平=1
                     按键闭合 ==》为低电平=0
    */
   /*
   atomic_set是什么意思——原子操作：即这个操作要么完整发生，要么不发生，不会写到一半被打断
   因为将来 v2 要加中断。中断可能在 read 执行的任何时刻突然插进来，也去写 key_value。
   如果不用原子操作，两个"写"可能互相踩，写坏数据。
   atomic_set的作用即是将KEY0VALUE写入(*dev).key_value
   */
    if (gpio_get_value(dev->key_gpio) == 0)
        atomic_set(&dev->key_value, KEY0VALUE);
    else
        atomic_set(&dev->key_value, INVAKEY);

    value = atomic_read(&dev->key_value);
    // 将内核空间中的value(一字节)拷贝到用户空间的buf中
    /*
    copy_to_user返回的是还有多少字节没有拷贝过去。
    这是一种“进度报告”式的返回值而不是“状态指示”
    */ 
    if (copy_to_user(buf, &value, sizeof(value)))
        return -EFAULT;              /* 雷1修复：失败返回负错误码 */

    return sizeof(value);            /* 雷1修复：成功返回实际字节数 */
}
/*
与应用层的close相对应
release在最后一个使用这放手时被调，而不是第一个close
*/
static int key_release(struct inode *inode, struct file *filp)
{
    return 0;                        /* 雷4修复：open/release 成对出现 */
}

static const struct file_operations key_fops = {
    .owner   = THIS_MODULE,
    .open    = key_open,
    .read    = key_read,
    .release = key_release,
};





// =======驱动五大块第11111块=======
// 模块入口
static int __init mykey_init(void)
{
    // 使用ret存储错误编码，通常错误编码是小于0的
    int ret;

    /* 1. 申请设备号（死分支已删除：动态申请是唯一路径） */
    // 申请设备号，成功返回0，失败返回负错误码
    ret = alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
    if (ret)
        return ret;
    keydev.major = MAJOR(keydev.devid);
    keydev.minor = MINOR(keydev.devid);
    printk("major=%d minor=%d\n", keydev.major, keydev.minor);

    /* 2. 硬件资源（半个 probe） */
    // 注意：cdev_init; THIS_MODULE与cdev_add这三个流程三位一体，同时出现，同时调用
    /*
    keyio_init()正式我们前期定义的GPIO口初始化函数
    其作用是申请独占GPIO口并改变GPIO的输入方向
    详细定义可以查看之前的函数定义
    */ 
    ret = keyio_init();
    if (ret)
        goto fail_gpio;              /* 错误路径逐级回退，见下 */

    /* 3. 注册 cdev */
    // 这行代码的作用是将key_fops的地址写入keydev.cdev
    cdev_init(&keydev.cdev, &key_fops);
    /*
    THIS_MODULE相当于一个身份证
    从这一刻起，内核可以通过(*cdev).owner找到这个cdev属于哪个模块
    相当于进行身份注册
    */
    keydev.cdev.owner = THIS_MODULE;
    /*
    cdev_add返回值0代表着cdev_add这个函数已经被成功执行
    keydev这个结构体中，devid存储的是主设备号与次设备号
    cdev_add的作用是将设备号存储进一个类似于哈希表的表格中
    将程序与设备进行对应于调用
    */
    ret = cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);

    if (ret)
        goto fail_cdev;

    /* 4. class + device → 自动生成 /dev/mykey，免 mknod */
    // 由于该项目是单实例的，正常的项目中应该是一个class对应多个device
    keydev.class = class_create(THIS_MODULE, KEY_NAME);
    if (IS_ERR(keydev.class)) {
        ret = PTR_ERR(keydev.class);
        goto fail_class;
    }

    keydev.device = device_create(keydev.class, NULL, keydev.devid,
                                  NULL, KEY_NAME);
    if (IS_ERR(keydev.device)) {
        ret = PTR_ERR(keydev.device);
        goto fail_device;
    }

    atomic_set(&keydev.key_value, INVAKEY);   /* 初始状态 = 无效键 */
    printk("mykey init ok\n");
    return 0;

/* 错误路径：按申请的逆序逐级释放——和 DHT11 remove 的对称思想一致 */
fail_device:
    class_destroy(keydev.class);
fail_class:
    cdev_del(&keydev.cdev);
fail_cdev:
    gpio_free(keydev.key_gpio);
fail_gpio:
    unregister_chrdev_region(keydev.devid, KEY_CNT);
    return ret;
}





// 模块出口
static void __exit mykey_exit(void)
{
    device_destroy(keydev.class, keydev.devid);   /* 逆序：先设备 */
    class_destroy(keydev.class);                  /* 再类 */
    cdev_del(&keydev.cdev);                       /* 再 cdev */
    gpio_free(keydev.key_gpio);                   /* 再硬件资源 */
    unregister_chrdev_region(keydev.devid, KEY_CNT);  /* 最后设备号 */
    printk("mykey exit\n");
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
