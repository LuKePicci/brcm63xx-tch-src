#!/bin/bash
REPO_ROOT="$1"
ARCHIVE_ROOT="$2"

if [ ! -d "$REPO_ROOT" ] || [ ! -d "$ARCHIVE_ROOT" ]
then
    echo "$0 REPO_ROOT ARCHIVE_ROOT"
    exit 1
fi

######################################
echo -n "Cleaning up broadcom-bsp... "

rm -rf "$REPO_ROOT/extern/broadcom-bsp" && echo "done"
mkdir -p "$REPO_ROOT/extern/broadcom-bsp"

###############################
echo -n "Importing SDK userspace... "

cp -rf "$ARCHIVE_ROOT/userspace" "$REPO_ROOT/extern/broadcom-bsp/userspace" && echo "done"

####################################
echo -n "Importing SDK kernel modules... "

cp -rf "$ARCHIVE_ROOT/bcmdrivers" "$REPO_ROOT/extern/broadcom-bsp/bcmdrivers" && echo "done"

####################################
echo -n "Importing SDK shared... "

cp -rf "$ARCHIVE_ROOT/shared" "$REPO_ROOT/extern/broadcom-bsp/shared" && echo "done"

####################################
echo -n "Importing SDK rdp... "

cp -rf "$ARCHIVE_ROOT/rdp" "$REPO_ROOT/extern/broadcom-bsp/rdp" && echo "done"


##########################
false && {
echo "Patching the SDK..."

pushd "$REPO_ROOT/extern"
patch -p1 < "$REPO_ROOT/extern.patch"
popd
}



###########################################
[ -n "$KERNEL_A" ] || KERNEL_A=linux-4.1.38
KERNEL_A_DIR="$REPO_ROOT/dl/$KERNEL_A"
echo "a <- $KERNEL_A_DIR"
[ -d ${KERNEL_A_DIR} ] || {
  echo -n "Preparing original kernel... "
  [ -f ${KERNEL_A_DIR}.tar.xz ] || {
    echo -e "failed\n\tmissing tarball for KERNEL_A at ${KERNEL_A_DIR}.tar.xz" && return
  }
  tar -xf ${KERNEL_A_DIR}.tar.xz -C ${REPO_ROOT}/dl && echo "done"
}

[ -n "$KERNEL_B" ] || KERNEL_B=linux-4.1
KERNEL_B_DIR="${ARCHIVE_ROOT}/kernel/$KERNEL_B"
echo "b <- ${KERNEL_B_DIR}"
echo -n "Diffing kernel... "
[ -d ${KERNEL_B_DIR} ] || {
  echo -e "failed\n\tKERNEL_B (${KERNEL_B}) not found at ${KERNEL_B_DIR}" && return
}

rm a b
ln -s ${KERNEL_A_DIR} a
ln -s ${KERNEL_B_DIR} b

echo
diff -ruN --no-dereference a/ b/ | tee /tmp/_generating_bcm.patch | stdbuf -o0 grep -e "+++" --binary-files=text --color=yes \
                                 | stdbuf -o0 cut -f 1 | stdbuf -o0 paste -sd "\r" && echo "done"
_output_patch=$REPO_ROOT/target/linux/brcm63xx-tch/000-Broadcom_bcm963xx_sdk_$(md5sum /tmp/_generating_bcm.patch|cut -c -6).patch
mv -f /tmp/_generating_bcm.patch $_output_patch

echo -e "\nYou now need to:"
echo -e "\t- rename ${REPO_ROOT}/extern/broadcom-bsp to something different (e.g. the tch source tarball name)"
echo -e "\t- create/define a new subtarget for this board if missing"
echo -e "\t- move ${_output_patch} into proper subtarget"
echo -e "\t- remove temp vanilla linux kernel folder, if no longer needed, from ${KERNEL_A_DIR}"
echo
