#!/bin/sh
modpath=$*
for drvpath in `echo $modpath`
do
	if [ -d $drvpath/drv ]
	 then
		for driver in `find $drvpath/drv -type f -a ! -name '*.conf' -print`
		do
			modload $driver > /tmp/loaderrs 2>&1
		done
	fi
done
rm -f /tmp/loaderrs
