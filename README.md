# About
The sole purpose of this project is to support developing a somewhat custom firmware for the L09G smart speaker. This project will be removed once https://github.com/duhow/xiaoai-patch/issues/49 has been completed.

Before continuing using any of the repos contents please read all provided documentation carefully. Please be aware of and acknowledge the following points:
1. **I am not responsible for your actions.**
1. **This project is highly experimental and has a high risk of rendering your device useless.**
1. **Do not use this software if you don't know what you are doing or if you can't effort bricking your device.**
1. I will not accept any feature or pull requests.
1. All software is developed to be using on linux.

# Component description
The following section describes the repo contents in detail.

## `flash.cpp`
Compile using `g++ flash.cpp -o flash`. When the L09G board is in `u-boot` mode, run using: `sudo ./flash __PATH_TO_DEVICE__ __SYSTEM_IMAGE_FILE__`.

The program can be used to flash the system partition with a custom `system.img` file using nothing but the serial port of the L09G. Programming the system partition with the ~9MB stock image file takes about 6 hours. The program performs all required steps to flash the system partition:

1. Load system image file `__SYSTEM_IMAGE_FILE__` into the RAM of the L09G. This is done by writing 16 byte chunks and immediately reading them back to verify, that the data has been written correctly.
2. Format `system` NAND partition
3. Write new system image from RAM to NAND.
4. Reboot

Manual user input/confirmation is required at step 2 and 4.

The default configuration of this program is the dry-run mode where all serial commands are printed to `stdout` but not send to the serial port. To enable the *real* programming mode, comment out the `DRY_RUN` define and recompile.

## `generate_system_image.sh`
This script can be used to generate a modified `system.img` file with the SSH server enabled. The script takes two parameters, the first one is the stock `system.img` file extracted from the `mico` update file, the second parameter is the output file.


Usage: `sudo ./generate_system_image.sh __ORIGINAL_SYSTEM_IMG_FILE__ __MODIFIED_SYSTEM_IMG_FILE__`.
