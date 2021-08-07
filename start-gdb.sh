:

SCRIPT_ROOT=`dirname "$0"`
GNU_INSTALL_ROOT=${GNU_INSTALL_ROOT:-${HOME}/tools/arm/gcc-arm-none-eabi-10.3-2021.07/bin}
GDB=${GNU_INSTALL_ROOT}/arm-none-eabi-gdb
DEFAULT_APP=./build/ble-pkap-responder-app.out
STARTUP_CMDS=${SCRIPT_ROOT}/gdb-startup-cmds.txt

if [ $# -eq 0 ]; then
    APP=${DEFAULT_APP}
else
    APP=$1
fi

EXTRA_STARTUP_CMDS=()
if [[ ! -z "$OPENTHREAD_ROOT" ]]; then
    EXTRA_STARTUP_CMDS+=(
        "set substitute-path /build/KNGP-TOLL1-JOB1/openthread/examples/.. ${OPENTHREAD_ROOT}"
    )
fi

exec ${GDB} -q -x ${STARTUP_CMDS} "${EXTRA_STARTUP_CMDS[@]/#/-ex=}" ${APP}
