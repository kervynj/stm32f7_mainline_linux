#/bin/sh
#variables and directories

PROJECT_DIR=`pwd`
LINUX_DIR=$PROJECT_DIR/linux-5.6-rc5
AFBOOT_DIR=$PROJECT_DIR/afboot-stm32
STLINK_DIR=$(dirname $PROJECT_DIR)/stlink.git # edit this as required

ROOTFS_DIR=$PROJECT_DIR/rootfs
DPCMD_DIR="/home/jim/Downloads/SF100Linux-1.2.1.03"
TOOLCHAIN=$PROJECT_DIR/tools/gcc-arm-none-eabi-4_9-2015q3/bin/arm-none-eabi-
S=$EUID;

#defaults 
BOARD="stm32f756-lightning"
ENV="all"
BINARY=$PROJECT_DIR/arch/arm/boot/xipImage.bin
#ADDR="QSPI"


usage()
{
	echo "No arguments given. 
	Usage:
		--target={board target}, default stm32f756-lightning
		--build={build target}, default all: options linux/afboot/rootfs/all
		--address={location to be written}: options QSPI/phsyical addr"
	exit 1
}


build_linux()
{
	# make Linux
	cd $PROJECT_DIR
	#check for linux directory
	if [ -d $LINUX_DIR ]
	then
		echo "Linux 5.6-rc5 exists"
	else
		echo "Downloading Linux 5.6-rc5"
		$(wget https://github.com/torvalds/linux/archive/v5.6-rc5.tar.gz)
		tar -xzvf "v5.6-rc5.tar.gz"
	#patch device-tree updates
	cp configs/dtb/stm32f7-pinctrl.dtsi  $LINUX_DIR/arch/arm/boot/dts/
	cp configs/dtb/stm32f769-disco.dts   $LINUX_DIR/arch/arm/boot/dts/
	cp configs/dtb/stm32f746.dtsi   $LINUX_DIR/arch/arm/boot/dts/

	fi

	#make kernel
	cd $LINUX_DIR
	CONFIG=$PROJECT_DIR/configs/$BOARD
	CONFIG=$CONFIG
	cp $CONFIG .config
	make ARCH=arm CROSS_COMPILE=$TOOLCHAIN -j 10
	cat arch/arm/boot/xipImage > arch/arm/boot/xipImage.bin
}


build_afboot()
{
	BRD=$1
	cd $AFBOOT_DIR
	make $BRD CROSS_COMPILE=$TOOLCHAIN 
}


build_rootfs()
{
	#make rootfs.cpio
	chown -h -R 0:0 $ROOTFS_DIR
	cd $ROOTFS_DIR && find . | cpio --quiet -o -H newc > $PROJECT_DIR/rootfs.cpio
}


build() # $1 environment, $2 board
{

	BINARY="" # binary to be flashed later if --flash exists

	case $1 in

		afboot)
			build_afboot $2
			BINARY=$AFBOOT_DIR/$BOARD.bin
			;;
		linux)
			build_linux
			BINARY=$LINUX_DIR/arch/arm/boot/xipImage.bin
			;;
		rootfs)
			build_rootfs
			BINARY=$LINUX_DIR/arch/arm/boot/xipImage.bin
			;;
		all)
			build_afboot $2
			build_rootfs
			build_linux
			BINARY=$LINUX_DIR/arch/arm/boot/xipImage.bin
			;;
        *)
            echo "ERROR: unknown build target of \"$1\""
            echo "Options are: {afboot, linux, rootfs, all}"
            exit 1
            ;;
    esac
    shift

}


flash()
{
	FILE=$2 #binary to be flashed
	ADDRESS=$3
	case $1 in

		afboot)
			$STLINK_DIR/build/Release/st-flash erase		
			$STLINK_DIR/build/Release/st-flash write $FILE $ADDRESS #0x08000000	
			$STLINK_DIR/build/Release/st-flash write $LINUX_DIR/arch/arm/boot/dts/stm32f769-disco.dtb 0x8008000	
			;;
		linux|all)
			if [ "$3" = "QSPI" ]; then
				#flash QSPI
				cd $DPCMD_DIR
				./dpcmd -e -v -p $FILE
			else
				$STLINK_DIR/build/Release/st-flash write $FILE $ADDRESS
			fi 
			;;
        *)
            echo "ERROR: unknown target for flashing of \"$1\""
            echo "Options are: {afboot, linux}"
            exit 1
            ;;
    esac
    shift
}


if [ $S -ne 0 ]; then
	echo "You are not root!";
	exit 0
fi


while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit
            ;;
        --target)
            BOARD=$VALUE
            ;;
        --build)
            ENV=$VALUE
            ;;
        --address)
			ADDR=$VALUE
			;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done

build $ENV $BOARD

if [ ! -z "$ADDR" ]; then 
	flash $ENV $BINARY $ADDR
fi
