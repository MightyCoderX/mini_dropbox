#!/usr/bin/env bash

docker run --rm \
	-v ./server:/server \
	-v ~/.local/share/nvim/mason/packages/codelldb/extension/lldb/bin/lldb-server:/bin/lldb-server \
	-p 13000:13000 \
	-p 1234:1234 \
	mini_dropbox_server_test:latest \
	lldb-server gdbserver '*:13000' /server
