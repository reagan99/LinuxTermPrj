cmd_/home/reagan9184/linux_term/chat2/Module.symvers :=  sed 's/ko$$/o/'  /home/reagan9184/linux_term/chat2/modules.order | scripts/mod/modpost -m -a -E   -o /home/reagan9184/linux_term/chat2/Module.symvers -e -i Module.symvers -T - 
