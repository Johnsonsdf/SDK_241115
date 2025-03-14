#!/bin/bash

ROOT_DIR=$PWD
SUBMOD_ROOT='zephyr framework/bluetooth framework/media framework/display'

echo -e "\n--== git cleanup ==--\n"
west forall -c "git reset --hard"
west forall -c "git clean -d -f -x"

echo -e "\n--== git update ==--\n"
list=`west config manifest.file`
git fetch
if [ "$list" == "west.yml" ]; then
	git rebase
fi
west update

echo -e "\n--== submodule update ==--\n"
for i in $SUBMOD_ROOT; do
	subdir=$ROOT_DIR/../${i}
	cd $subdir
	echo -e "\n--== $subdir update ==--\n\n"
	git submodule init
	git submodule update
	if [ $? -ne 0 ]; then
		git submodule deinit --all -f
	fi
done

cd $ROOT_DIR
