#!bash  
  ./.env/bin/python Middlewares/mcuboot/scripts/imgtool.py sign \
    --key application/boot/root-ec-p256.pem \
    --header-size 0x20 \
    --pad-header \
    --align 8 \
    --slot-size 0x1B800 \
    --version 1.0.0 \
    --pad \
    build/Debug/mcu_boot.bin build/Debug/app_signed.bin