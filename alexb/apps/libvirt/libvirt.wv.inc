# -*-sh-*-

nulroot=$(readlink -f $(dirname $0)/../../..)
wvpath=$nulroot/michal/wvtest

. $wvpath/wvtest.sh

_args="$*"

init() {
    # Setup libvirt environment
    libvirt=$nulroot/alexb/apps/libvirt
    WVPASS make -C $libvirt install DESTDIR=$libvirt/_install
    WVPASS cp $nulroot/michal/imgs/passive/CAcertSrv.pem cacert.pem
    set -x
    PATH=$libvirt/_install/usr/local/bin:$PATH
    LD_LIBRARY_PATH=$libvirt/_install/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    set +x
    export LD_LIBRARY_PATH


    if [ -z "$NOVA_IP" ]; then
        # Boot NOVA up to the point it is receiving libvirt connections
	export WVTEST_EXIT_PATTERN="NOVA management daemon is up. Waiting for libvirt connection"
	passive=$nulroot/michal/boot/passive
	if ! echo " $_args"|grep -Eq '(--server|-I)'; then # It seems that the test will run in qemu
	    passive=${passive}-ne2k
	    export WVTEST_NOKILL=1
	    NOVA_IP=127.0.0.1
	fi
	set -x
	$wvpath/wvnulrun $passive $_args | tee passive.log
	test "$NOVA_IP" || NOVA_IP=$(sed -ne '/.*update  - got ip=\([^ ]*\).*/ s//\1/p' passive.log)
	pid=$(sed -ne 's/^Keeping PID \([0-9]\+\) alive$/\1/p' passive.log)
	set +x
	WVPASS test $NOVA_IP
    fi
}

finish() {
    if [ "$pid" ]; then kill $pid; fi
}
