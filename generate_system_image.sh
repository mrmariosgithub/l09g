#!/bin/bash -e

# Check for root
if [[ "$(whoami)" != 'root' ]]
then
	echo "ERROR: Only root can do this"
	exit 1
fi

# Check parameters
if [ $# -ne 2 ]
then
	echo "USAGE: $0 __INPUT_FILE__ __OUTPUT_FILE__"
	exit 1
fi

# Store parameter
INPUT_FILE="$1"
OUTPUT_FILE="$2"
TEMP_FOLDER="squash"

# Cleanup
if [ -d "${TEMP_FOLDER}" ]
then
	rm -rf "${TEMP_FOLDER}"
fi

# Unpack squashfs
echo "Unpacking squashfs"
unsquashfs -d "${TEMP_FOLDER}" "${INPUT_FILE}" > /dev/null

# Work in subshell
(
	# Enter unpacked folder
	cd "${TEMP_FOLDER}"

	# Enable SSHD
	(
		echo 'echo "================================================================"'
		echo '/usr/bin/sshd -G -r /etc/rsa_host_key'
		echo 'echo "================================================================"'
	) >> usr/bin/set_usb_serialnumber.sh
)

# Repack squashfs
echo "Repacking squashfs"
mksquashfs "${TEMP_FOLDER}" "${OUTPUT_FILE}" -comp xz -noappend -all-root -always-use-fragments -b 131072 > /dev/null
