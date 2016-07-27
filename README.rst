Description
===========
Umeq is an equivalent of `Qemu <http://wiki.qemu.org/Main_Page>`_ user mode.
It allows you to run foreign architecture binaries on your host system.
For example you can run arm64 binaries on x86_64 linux desktop.

You can find compile umeq static binaries `here <https://github.com/mickael-guene/umeq/releases>`_. You can also directly use `docker images <https://hub.docker.com/u/mickaelguene/>`_.

Build status
============
.. image:: https://travis-ci.org/mickael-guene/umeq.svg?branch=master
    :target: https://travis-ci.org/mickael-guene/umeq

How to use it
=============

With `Docker <https://www.docker.com/>`_
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Once you have docker installed just run :
::

 > docker run -i -t --rm mickaelguene/arm64-debian
 root@4941c21b263e:/# uname -a && cat /etc/os-release
 Linux 4941c21b263e 4.6.0-1-amd64 #1 SMP Debian 4.6.3-1 (2016-07-04) aarch64 GNU/Linux
 PRETTY_NAME="Debian GNU/Linux 8 (jessie)"
 NAME="Debian GNU/Linux"
 VERSION_ID="8"
 VERSION="8 (jessie)"
 ID=debian
 HOME_URL="http://www.debian.org/"
 SUPPORT_URL="http://www.debian.org/support"
 BUG_REPORT_URL="https://bugs.debian.org/"
 root@4941c21b263e:/# exit
 exit
 >

If you have binfmt_misc support then you can start with any command like that :
::

 > docker run -i -t --rm mickaelguene/arm64-debian uname -a
 Linux 0cd520698e0e 4.6.0-1-amd64 #1 SMP Debian 4.6.3-1 (2016-07-04) aarch64 GNU/Linux

Else you need to specify umeq command line :
::

 > docker run -i -t --rm mickaelguene/arm64-debian umeq-arm64 -execve -0 uname /bin/uname -a
 Linux 0cd520698e0e 4.6.0-1-amd64 #1 SMP Debian 4.6.3-1 (2016-07-04) aarch64 GNU/Linux

With `Proot <http://proot.me>`_
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Running umeq along with PRoot allows you to execute foreign architecture binaries on your host system.
The foreign application will be isolated in his original root file system.

In a chroot environment with or without binfmt support
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
If you have the necessary rights, you can use umeq in a chroot environment using
binfmt kernel support to run umeq transparently.

Standalone
^^^^^^^^^^
Umeq can also be use as a standalone binary with guest static binaries.

Current Status
==============
Host support
^^^^^^^^^^^^
For the moment umeq support following host architecture:
- x86_64 host is in release state.
- i386 host is in beta state. Moreover in that case only 32 bits guest are
supported.

Guest support
^^^^^^^^^^^^^
For the moment umeq support following guest architecture:

- arm64 support is in release state.
- armv7 support is in beta state.

Proot compatibility
^^^^^^^^^^^^^^^^^^^
For maximum features support and stability there is umeq patches against proot.
You can still use official proot release but you need to consider these points:

+ To workaround a kernel `bug <https://bugzilla.kernel.org/show_bug.cgi?id=91791>`_ you need to setup stack limit size in your shell before launching proot

::

 > ulimit -Ss 16386

+ Ptrace emulation will not work correctly. This means strace and gdb will not work properly.

If you don't want such limitations then you need to use a statically build `custom Proot <https://raw.githubusercontent.com/mickael-guene/proot-static-build/master-umeq/static/proot-x86_64>`_ which contain umeq patches to run umeq without any limitations.

How to build umeq
=================
::

 > git clone https://github.com/mickael-guene/umeq.git
 > cd umeq
 > mkdir build
 > cd build
 > cmake ..
 > make all
 > make test

Arm64 rootfs
==============
gentoo
^^^^^^
You can find a gentoo arm64 rootfs `here <http://gentoo.osuosl.org/experimental/arm/arm64/stage3-arm64-20140718.tar.bz2>`_

debian
^^^^^^
you need to build it using proot, umeq and multistrap

First create a multistrap.conf file with following content
----------------------------------------------------------
::

 [General]
 noauth=true
 unpack=true
 debootstrap=Debian
 aptsources=Debian
 
 [Debian]
 packages=apt build-essential
 source=http://ftp.debian.org/debian
 suite=sid

Call Multistrap to create rootfs
--------------------------------
::

 >x86_64: multistrap -a arm64 -d sid -f multistrap.conf
 ....
 >x86_64: proot -0 -b /proc -b /dev -r sid/ -q umeq-arm64 bash
 >aarch64: export PATH=/usr/local/sbin:/usr/sbin:/sbin:${PATH}
 >aarch64: export DEBIAN_FRONTEND=noninteractive
 >aarch64: export DEBCONF_NONINTERACTIVE_SEEN=true
 >aarch64: export LC_ALL=C LANGUAGE=C LANG=C
 >aarch64: /var/lib/dpkg/info/dash.preinst install
 Adding 'diversion of /bin/sh to /bin/sh.distrib by dash'
 Adding 'diversion of /usr/share/man/man1/sh.1.gz to /usr/share/man/man1/sh.distrib.1.gz by dash'
 >aarch64: dpkg-divert --local --rename --add /usr/sbin/adduser
 Adding 'local diversion of /usr/sbin/adduser to /usr/sbin/adduser.distrib'
 >aarch64: ln -s /bin/true /usr/sbin/adduser
 >aarch64: dpkg --configure -a
 Setting up gcc-4.8-base:arm64 (4.8.4-1) ...
 ...
 Processing triggers for libc-bin (2.19-13) ...
 >aarch64: rm /usr/sbin/adduser
 >aarch64: dpkg-divert --rename --remove /usr/sbin/adduser
 Removing 'local diversion of /usr/sbin/adduser to /usr/sbin/adduser.distrib'
 >aarch64: DEBIAN_FRONTEND=readline dpkg-reconfigure dash
 Configuring dash
 ----------------
 
 The system shell is the default command interpreter for shell scripts.
 
 Using dash as the system shell will improve the system's overall performance. It does not alter the shell presented to interactive users.
 
 Use dash as the default system shell (/bin/sh)? n
 
 
 Removing 'diversion of /bin/sh to /bin/sh.distrib by dash'
 Adding 'diversion of /bin/sh to /bin/sh.distrib by bash'
 Removing 'diversion of /usr/share/man/man1/sh.1.gz to /usr/share/man/man1/sh.distrib.1.gz by dash'
 Adding 'diversion of /usr/share/man/man1/sh.1.gz to /usr/share/man/man1/sh.distrib.1.gz by bash'
 >aarch64: exit
 >x86_64:

Examples usage
==============
With proot
^^^^^^^^^^
Following command will drop you into a arm64 bash shell::

 > proot -R <arm64_rootfs_dir> -q umeq-arm64 bash
 > uname -m
 aarch64

In a chroot environment
^^^^^^^^^^^^^^^^^^^^^^^

tbd

Standalone
^^^^^^^^^^
::

 > umeq-arm64 <umeq_source_dir>/test/static/arm64/opcode/base/a
