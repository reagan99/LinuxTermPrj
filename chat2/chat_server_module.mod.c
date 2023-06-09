#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x4687b179, "sock_release" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x3917500d, "iov_iter_kvec" },
	{ 0xff499308, "kernel_sendmsg" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x315d3f3, "kernel_recvmsg" },
	{ 0x92997ed8, "_printk" },
	{ 0xe1ad35a9, "init_net" },
	{ 0x5e361196, "sock_create_kern" },
	{ 0xaf8641f, "kernel_bind" },
	{ 0x52b43b78, "kernel_listen" },
	{ 0x3bbfa47f, "kernel_accept" },
	{ 0x5a921311, "strncmp" },
	{ 0x9166fada, "strncpy" },
	{ 0x6e64aaf, "filp_open" },
	{ 0xcad22683, "kmalloc_caches" },
	{ 0xdb776aa1, "kmalloc_trace" },
	{ 0x4228868d, "filp_close" },
	{ 0x6c1f55c0, "kernel_read" },
	{ 0x37a0cba, "kfree" },
	{ 0xa916b694, "strnlen" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0x54ef0a4f, "param_ops_int" },
	{ 0xbf8e4f2e, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "E136EBD4C227862C3EC6616");
