cmd_/home/reagan9184/linux_term/hello/hello.mod := printf '%s\n'   hello.o | awk '!x[$$0]++ { print("/home/reagan9184/linux_term/hello/"$$0) }' > /home/reagan9184/linux_term/hello/hello.mod
