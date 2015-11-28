#!/bin/sh

rm -rf /var/tmp/messip
mkdir -p /var/tmp/messip/usr/lib
cp Release/libmessip.so  /var/tmp/messip/usr/lib
strip /var/tmp/messip/usr/lib/libmessip.so

mkdir -p /var/tmp/messip/usr/bin
cp ../messip_mgr/Release/messip_mgr  /var/tmp/messip/usr/bin
strip /var/tmp/messip/usr/bin/messip_mgr

sudo rm -f /usr/src/redhat/RPMS/i386/messip-*
sudo touch /usr/src/redhat/RPMS/i386/messip-0-9.i386.rpm
sudo chmod a+rw /usr/src/redhat/RPMS/i386/messip-0-9.i386.rpm

rpmbuild -bb --quiet messip.spec
