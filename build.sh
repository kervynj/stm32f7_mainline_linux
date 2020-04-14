PROJECT_DIR=`pwd`
LINUX_DIR="linux-5.6-rc5"
AFBOOT_DIR="afboot-stm32"
STLINK_DIR="stlink.git"
BOARD="stm32f756-lightning"
ROOTFS_DIR="rootfs"
TOOLCHAIN=$PROJECT_DIR/tools/gcc-arm-none-eabi-4_9-2015q3/bin/arm-none-eabi-
S=$EUID;

if [ $S -ne 0 ]; then
	echo "You are not root!";
	exit 0
fi
	
#make af-boot
cd $AFBOOT_DIR
make $BOARD CROSS_COMPILE=$TOOLCHAIN 

cd $PROJECT_DIR
#check for linux directory
if [ -d $LINUX_DIR ]
then
	echo "Linux 5.6-rc5 exists"
else
	echo "Downloading Linux 5.6-rc5"
	$(wget https://github.com/torvalds/linux/archive/v5.6-rc5.tar.gz)
	tar -xzvf "v5.6-rc5.tar.gz"
fi


#make rootfs.cpio
chown -h -R 0:0 $PROJECT_DIR/$ROOTFS_DIR
cd $PROJECT_DIR/$ROOTFS_DIR && find . | cpio --quiet -o -H newc > $PROJECT_DIR/rootfs.cpio
#ls | cpio -ov > rootfs.cpio

#make kernel
cd $PROJECT_DIR/$LINUX_DIR
#cp $PROJECT_DIR/configs/$BOARD .config
make ARCH=arm CROSS_COMPILE=$TOOLCHAIN -j 10
cat $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage > $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin


cd $PROJECT_DIR
#flash to target
../$STLINK_DIR/build/Release/st-flash erase
../$STLINK_DIR/build/Release/st-flash write $PROJECT_DIR/$AFBOOT_DIR/$BOARD.bin 0x08000000
#../$STLINK_DIR/build/Release/st-flash write $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin 0x08010000
../$STLINK_DIR/build/Release/st-flash write $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/dts/stm32f769-disco.dtb 0x8008000
