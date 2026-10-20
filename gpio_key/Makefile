# 告诉内核系统要造什么
# obj是内核Kbuild系统的保留变量
# -m代表编成模块Module
obj-m += gpio_key.o

# 内核源码树的绝对路径，外部模块编译的本质是：
# “把你的 .c 文件，用板子上那个内核的构建系统、按那个内核的配置，编一份出来”。
KDIR := /home/aamiao/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
# 使用shell生成的相对路径，更换项目路径时，不需要更改Makefile文件
# 但是代价是Makefile文件必须放在项目的主路径下
PWD  := $(shell pwd)
# 本次交叉编译的核心，指定编译架构
# 若不指定，则会直接按照机的架构进行编译
ARCH := arm
# 交叉编译前缀，CROSS_COMPILE必须于ARCH同时出现
CROSS_COMPILE := arm-linux-gnueabihf-

# 之后的编译路径与相关参数只需要参考上文的设置即可
all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
