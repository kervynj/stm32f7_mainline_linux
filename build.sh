PROJECT_DIR=`pwd`
LINUX_DIR="stm32"
AFBOOT_DIR="afboot-stm32"
BOARD=stm32f412-disco
ROOTFS_DIR="build"
DPCMD="../SF100Linux/dpcmd"
S=$EUID;

if [ $S -ne 0 ]; then
	echo "You are not root!";
	exit 0
fi
	
#make af-boot
cd $AFBOOT_DIR
make $BOARD 

#make kernel
cd $PROJECT_DIR/$LINUX_DIR
#make ARCH=arm CROSS_COMPILE=arm-none-eabi- stm32_defconfig
make ARCH=arm CROSS_COMPILE=arm-none-eabi- CONFIGS=$PROJECT_DIR/configs/$BOARD -j 4

cat $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage > $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin

#generate combined QSPI image
COMBINEDFILE=$PROJECT_DIR/output.bin
dd if=/dev/zero bs=$((1024*1024*16)) count=1 | tr "\000" "\377" > $COMBINEDFILE
dd conv=notrunc bs=1 if=$PROJECT_DIR/$LINUX_DIR/arch/arm/boot/dts/stm32f429-disco.dtb of=$COMBINEDFILE seek=$((0x04000))
dd conv=notrunc bs=1 if=$PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin of=$COMBINEDFILE seek=$((0x08000))


#make rootfs.cpio
cd $PROJECT_DIR/$ROOTFS_DIR
#sudo find . | cpio --quiet -o -H newc > $PROJECT_DIR/rootfs.cpio


cd $PROJECT_DIR
#flash to target

openocd -f  board/stm32f429discovery.cfg \
 -c "init" \
 -c "reset init" \
 -c "flash probe 0" \
 -c "flash info 0" \
 -c "flash write_image erase $PROJECT_DIR/$AFBOOT_DIR/$BOARD.bin 0x08000000" \
 -c "reset run" \
 -c "shutdown"

# -c "flash write_image erase $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin 0x08008000" \
# -c "flash write_image erase $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/dts/stm32f429-disco.dtb 0x08004000" \

flashrom -p dediprog -w output.bin
