#!/bin/sh
cmake --build build

tmux new-session -d -c tests '../build/server/server'
tmux split-window -h -c  tests '../build/client/client client 127.0.0.1 20001'
tmux new-window 'mutt'
tmux -2 attach-session -d