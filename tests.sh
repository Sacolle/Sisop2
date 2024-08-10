#!/bin/sh
cmake --build build

tmux new-session -d -c tests/server '../../build/server/server ; input'
tmux split-window -h -c tests/clientA '../../build/client/client A 127.0.0.1 20001 ; input'
tmux split-window -h -c tests/clientB 'sleep 1 && ../../build/client/client A 127.0.0.1 20001 ; input'
tmux new-window 'mutt'
tmux -2 attach-session -d