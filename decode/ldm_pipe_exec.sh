#!/bin/sh
# ldm_pipe_exec - invoke an LDM decoder on a pipe, but return as soon as the
#		  input has been received -- not after it's been processed.
#		  This is necessary when slow decoders (e.g. lwtmd2) are
#		  invoked by an overloaded LDM.
#
tmp=/tmp/ldm_pipe_exec.$$
trap "rm -f \$$tmp; exit 1" 1 2 3 13 14 15
cat > $tmp
(eval "$@" < $tmp; rm $tmp) &
