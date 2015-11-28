#!/bin/sh

rm -rf /var/tmp/messip_devel
mkdir -p /var/tmp/messip_devel/usr/include
cp Src/messip.h  /var/tmp/messip_devel/usr/include

sudo rm -f /usr/src/redhat/RPMS/i386/messip_devel-*
sudo touch /usr/src/redhat/RPMS/i386/messip_devel-0-9.i386.rpm
sudo chmod a+rw /usr/src/redhat/RPMS/i386/messip_devel-0-9.i386.rpm

rpmbuild -bb --quiet messip_devel.spec
