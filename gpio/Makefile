module_name=gpio
obj-m += $(module_name).o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insmod: all rmmod
	sudo insmod $(module_name).ko
	sudo chmod 0666 /dev/gpio_dev

rmmod:
	sudo rmmod $(module_name) || true

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	
