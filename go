#!/bin/bash

#export FORCE_KERNEL_VERMAGIC="0b3a287109a01f28a0c5f067991f321a"
#export FORCE_KERNEL_VERMAGIC="1cc0b47f4eacd68f62a475ee32bcdc5f"
make -j10 V=s $@
