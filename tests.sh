#!/bin/sh
cmake --build build

tmux new-session -d -c tests/server '../../build/server/server 1 2 21001 21002 21003 < server1.txt; input'
tmux split-window -h -c tests/server '../../build/server/server 2 2 22001 22002 22003 < server2.txt; input'
tmux split-window -h -c tests/server '../../build/server/server 3 2 23001 23002 23003 < server3.txt; input'
#tmux split-window -h -c tests/clientA '../../build/client/client A 127.0.0.1 23001 ; input'
tmux new-window 'mutt'
tmux -2 attach-session -d