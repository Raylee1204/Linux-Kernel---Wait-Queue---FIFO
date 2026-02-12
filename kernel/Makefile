# 指定要編譯的目標檔案名稱 (對應你的 .c 檔名)
obj-m += call_my_wait_queue.o

# 編譯指令
all:
	# 使用當前系統的 Kernel Headers 進行編譯
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# 清除指令
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
