// usb_activity_monitor.c

#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/device.h>

#define AUTHOR "Burak Ozter"
#define DESCRIPTION "USB Activity Monitor Kernel Module"

int usb_for_each_dev_callback(struct usb_device *, void *);
int list_usb_devices(void);

int usb_for_each_dev_callback(struct usb_device *device, void *ptr)
{
    if (device)
    {
        printk(KERN_INFO "USB Device: Bus %03d Device %03d: VID:PID = %04x:%04x\n",
               device->bus ? device->bus->busnum : -1,
               device->devnum,
               le16_to_cpu(device->descriptor.idVendor),
               le16_to_cpu(device->descriptor.idProduct));

        if (device->manufacturer)
            printk(KERN_INFO "  Manufacturer: %s\n", device->manufacturer);
        if (device->product)
            printk(KERN_INFO "  Product: %s\n", device->product);
        if (device->serial)
            printk(KERN_INFO "  Serial Number: %s\n", device->serial);
    }
    return 0;
}

int list_usb_devices(void)
{
    int data = 0;
    int res = usb_for_each_dev((void *)&data, usb_for_each_dev_callback);
    if (res)
    {
        printk(KERN_ERR "Failed to call usb_for_each_dev in module init.\n");
        return res;
    }

    return 0;
}

static int __init usb_activity_monitor_init(void)
{
    printk(KERN_INFO "Init usb_activity_monitor module\n");
    if (list_usb_devices())
    {
        printk(KERN_ERR "Error intitializing usb_activity_monitor kernel module.\n");
        return -1;
    }
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
