#!/bin/bash
__SYFCO=~/.local/bin/syfco

__TLSF_PATH=$1

if [[ ! -f "$__TLSF_PATH" ]]; then
  echo "file not found"
  exit 1 # error
fi

if [[ ! $($__SYFCO -c $__TLSF_PATH) ]]; then
  echo "file is not a valid TLSF file"
  exit 1 # error
fi

if [[ ! -f "$__TLSF_PATH.ins" ]]; then
  $__SYFCO -ins $__TLSF_PATH > $__TLSF_PATH.ins
fi

if [[ ! -f "$__TLSF_PATH.ltl" ]]; then
  $__SYFCO -s1 -f ltl $__TLSF_PATH > $__TLSF_PATH.ltl
fi

exit 0 # success