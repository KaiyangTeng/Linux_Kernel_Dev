obj-m += mp1.o

all:
	make -C /home/kteng4/linux-5.15.165 M=$(PWD) modules
	gcc -o userapp userapp.c

clean:
	make -C /home/kteng4/linux-5.15.165 M=$(PWD) clean
	$(RM) userapp
