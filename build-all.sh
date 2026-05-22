#! /bin/bash

set -euxo pipefail

export CHECK_SYMBOLS=no

make clean-all
./build32.sh
./build32-eh.sh
./build32-exnref-eh.sh
./build32-ehpic.sh
./build32-exnref-ehpic.sh

mkdir -p ~/.wasix-sysroot
rsync -Lrtv --delete ./sysroot32/ ~/.wasix-sysroot/sysroot
rsync -Lrtv --delete ./sysroot32-eh/ ~/.wasix-sysroot/sysroot-eh
rsync -Lrtv --delete ./sysroot32-exnref-eh/ ~/.wasix-sysroot/sysroot-exnref-eh
rsync -Lrtv --delete ./sysroot32-ehpic/ ~/.wasix-sysroot/sysroot-ehpic
rsync -Lrtv --delete ./sysroot32-exnref-ehpic/ ~/.wasix-sysroot/sysroot-exnref-ehpic

echo Done!
