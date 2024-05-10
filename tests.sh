#!/bin/sh
cmake --build build

tmux new-session -d -c tests '../build/server/server'
tmux split-window -h -c  tests '../build/client/client'
tmux new-window 'mutt'
tmux -2 attach-session -d