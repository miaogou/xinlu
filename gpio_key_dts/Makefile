# 该Makefile对之前的版本进行了改造


# 原来的Makefile的效果：
# make：编译驱动模块
# make clean：清理驱动产物


# 新版本Makefile的效果：
# make：同上版本编写驱动模块
# make clean：同上版本清理驱动产物
# make dtb：交叉编译设备树
# make verify：反编译dtb





# 告诉内核系统要造什么
# obj是内核Kbuild系统的保留变量
# -m代表编成模块Module
obj-m += gpio_key_dts.o

# 内核源码树的绝对路径，外部模块编译的本质是：
# "把你的 .c 文件，用板子上那个内核的构建系统、按那个内核的配置，编一份出来"。
KDIR := /home/aamiao/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
# 使用shell生成的相对路径，更换项目路径时，不需要更改Makefile文件
# 但是代价是Makefile文件必须放在项目的主路径下
PWD  := $(shell pwd)
# 本次交叉编译的核心，指定编译架构
# 若不指定，则会直接按照机的架构进行编译
ARCH := arm
# 交叉编译前缀，CROSS_COMPILE必须于ARCH同时出现
CROSS_COMPILE := arm-linux-gnueabihf-

# 设备树产物与反编译工具的路径
# DTB：上面dts编译后的产物（内核构建系统输出的最终二进制）
# DTC：内核树自带的反编译器，make dtbs内部调用的就是它——自产自用，不用另装
DTB   = $(KDIR)/arch/arm/boot/dts/imx6ull-14x14-emmc-7-1024x600-c.dtb
DTC   = $(KDIR)/scripts/dtc/dtc

# ============ 驱动侧 ============
# 之后的编译路径与相关参数只需要参考上文的设置即可
modules:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean

# ============ 设备树侧 ============
# 编译设备树（增量编译）
# 注意：这里不带M=$(PWD)——编的是内核树自己的dtbs，不是外部模块
# 带了M反而会出错，这是它和modules目标的关键区别
dtb:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) dtbs

# 反编译双验证：把dtb还原成dts，确认改动真的进了二进制
# 编译通过只说明语法没错，不说明内容没丢——所以验证是独立的一步
verify:
	@echo "===== [1/2] mykey 节点 ====="
	$(DTC) -I dtb -O dts $(DTB) | grep -A 8 "mykey"
	@echo "===== [2/2] gpio_keys_grp 组 ====="
	$(DTC) -I dtb -O dts $(DTB) | grep -B 1 -A 5 "gpio_keys_grp"

# ============ 一条龙 ============
# make裸跑的默认目标（make执行文件里第一个目标，所以all放最后也没关系，
# 只要它是"第一个被定义的目标"就行——这里靠.PHONY前的位置约定）
# 依赖链按顺序执行：驱动 → 设备树 → 验证，验证永远不会被跳过
all: modules dtb verify

.PHONY: all modules clean dtb verify
