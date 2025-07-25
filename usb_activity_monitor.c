// usb_activity_monitor.c

#include <linux/module.h>
#include <linux/init.h>

#define AUTHOR "Burak Ozter"
#define DESCRIPTION "USB Activity Monitor Kernel Module"

static int __init usb_activity_monitor_init(void)
{
    printk(KERN_INFO "Init usb_activity_monitor module\n");
    return 0;
}

static void __exit usb_activity_monitor_exit(void)
{
    printk(KERN_INFO "exit usb_activity_monitor module\n");
}

module_init(usb_activity_monitor_init);
module_exit(usb_activity_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
