#!/bin/sh
cmake --build build

cd tests
tmux new-session -d -c server '../build/server/server ; input'
tmux split-window -h -c client '../build/client/client client 127.0.0.1 20001 ; input'
tmux new-window 'mutt'
tmux -2 attach-session -d