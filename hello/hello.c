#include <linux/module.h>
MODULE_LICENSE("GPL");
static int hello_init(void)
{
    printk("hello world.\n");
    return 0;
}

static void hello_exit(void)
{
    printk("Goodbye, cruel world.\n");
}

module_init(hello_init);
module_exit(hello_exit);
