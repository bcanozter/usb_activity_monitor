// usb_activity_monitor.c

#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/notifier.h>

#define AUTHOR "Burak Ozter"
#define DESCRIPTION "USB Activity Monitor Kernel Module"

#define PROC_FILENAME "usb_activity_monitor"
static int print_usb_device_info(struct usb_device *device)
{
    if (device)
    {
        printk(KERN_INFO "USB Device: Bus %03d Device %03d: VID:PID = 0x%04x:0x%04x\n",
               device->bus ? device->bus->busnum : -1,
               device->devnum,
               le16_to_cpu(device->descriptor.idVendor),
               le16_to_cpu(device->descriptor.idProduct));

        if (device->manufacturer)
            printk(KERN_INFO "   Manufacturer: %s\n", device->manufacturer);
        if (device->product)
            printk(KERN_INFO "   Product: %s\n", device->product);
        if (device->serial)
            printk(KERN_INFO "   Serial Number: %s\n", device->serial);
        return 0;
    }
    return -1;
}

static int usb_notifier_callback(struct notifier_block *self,
                                 unsigned long action, void *dev)
{
    struct usb_device *udev = dev;
    switch (action)
    {
    case USB_DEVICE_ADD:
        printk(KERN_WARNING PROC_FILENAME ": USB device has been added.\n");
        print_usb_device_info(udev);
        break;
    case USB_DEVICE_REMOVE:
        printk(KERN_WARNING PROC_FILENAME ": USB device has been removed.\n");
        print_usb_device_info(udev);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block usb_notifier_block = {
    .notifier_call = usb_notifier_callback,
};

static int usb_for_each_dev_callback(struct usb_device *device, void *ptr)
{
    int err = print_usb_device_info(device);
    if (err)
    {
        return err;
    }
    return 0;
}

static int list_usb_devices(void)
{
    int data = 0;
    int res = usb_for_each_dev((void *)&data, usb_for_each_dev_callback);
    if (res)
    {
        printk(KERN_ERR PROC_FILENAME ": Failed to call usb_for_each_dev in module init.\n");
        return res;
    }

    return 0;
}

static int __init usb_activity_monitor_init(void)
{
    printk(KERN_INFO PROC_FILENAME ": Init usb_activity_monitor module\n");
    int err;
    err = list_usb_devices();
    if (err)
    {
        printk(KERN_ERR PROC_FILENAME ": Error intitializing usb_activity_monitor kernel module.\n");
        goto out;
    }
    usb_register_notify(&usb_notifier_block);
out:
    return err;
}

static void __exit usb_activity_monitor_exit(void)
{
    printk(KERN_INFO PROC_FILENAME ": usb_activity_monitor_exit\n");
    usb_unregister_notify(&usb_notifier_block);
}

module_init(usb_activity_monitor_init);
module_exit(usb_activity_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
