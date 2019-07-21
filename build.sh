PROJECT_DIR=`pwd`
LINUX_DIR="stm32"
AFBOOT_DIR="afboot-stm32"
BOARD=stm32f429i-disco

#make af-boot
cd $AFBOOT_DIR
make $BOARD 

#make kernel
cd $PROJECT_DIR/$LINUX_DIR
#make ARCH=arm CROSS_COMPILE=arm-none-eabi- stm32_defconfig
make ARCH=arm CROSS_COMPILE=arm-none-eabi- CONFIGS=$PROJECT_DIR/configs/$BOARD -j 4

cat $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage > $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin

cd $PROJECT_DIR
#flash to target

openocd -f  board/stm32f429discovery.cfg \
 -c "init" \
 -c "reset init" \
 -c "flash probe 0" \
 -c "flash info 0" \
 -c "flash write_image erase $PROJECT_DIR/$AFBOOT_DIR/$BOARD.bin 0x08000000" \
 -c "flash write_image erase $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/xipImage.bin 0x08008000" \
 -c "flash write_image erase $PROJECT_DIR/$LINUX_DIR/arch/arm/boot/dts/stm32f429-disco.dtb 0x08004000" \
 -c "reset run" \
 -c "shutdown"

