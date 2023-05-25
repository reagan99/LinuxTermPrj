cmd_/home/reagan9184/linux_term/client/client.mod := printf '%s\n'   client.o | awk '!x[$$0]++ { print("/home/reagan9184/linux_term/client/"$$0) }' > /home/reagan9184/linux_term/client/client.mod
