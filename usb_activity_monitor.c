// usb_activity_monitor.c

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>

#define AUTHOR "Burak Ozter"
#define DESCRIPTION "USB Activity Monitor Kernel Module"

#define PROC_FILENAME "usb_activity_monitor"

static DEFINE_MUTEX(rw_mutex);
static struct proc_dir_entry *proc_file;
static struct usb_device_info *usb_devices_array;
static int device_count = 0;
#define MAX_USB_DEVICES 100
#define MAX_STRING_LENGTH 255
struct usb_device_info
{
    int bus_num;
    int dev_num;
    u16 vendor_id;
    u16 product_id;
    char manufacturer[MAX_STRING_LENGTH];
    char product[MAX_STRING_LENGTH];
    char serial[MAX_STRING_LENGTH];
    bool is_connected;
    struct timespec64 added_time;
    struct timespec64 removed_time;
};

static int find_device_in_array(int bus_num, int dev_num)
{
    int i;
    for (i = 0; i < device_count; i++)
    {
        if (usb_devices_array[i].bus_num == bus_num &&
            usb_devices_array[i].dev_num == dev_num)
        {
            return i;
        }
    }
    return -1;
}

static int add_device_to_array(struct usb_device *device)
{
    // TODO: Either create a hard cap or circular linked list or something.
    //  MAX_USB_DEVICES is not respected. This will cause really bad things.
    int index = device_count; //?
    int ret;
    ret = mutex_trylock(&rw_mutex);
    if (ret != 0)
    {
        usb_devices_array[index].bus_num = device->bus ? device->bus->busnum : -1;
        usb_devices_array[index].dev_num = device->devnum;
        usb_devices_array[index].vendor_id = le16_to_cpu(device->descriptor.idVendor);
        usb_devices_array[index].product_id = le16_to_cpu(device->descriptor.idProduct);
        usb_devices_array[index].is_connected = true;

        if (device->manufacturer)
        {
            strncpy(usb_devices_array[index].manufacturer, device->manufacturer,
                    MAX_STRING_LENGTH - 1);
            usb_devices_array[index].manufacturer[MAX_STRING_LENGTH - 1] = '\0';
        }
        else
        {
            strcpy(usb_devices_array[index].manufacturer, "Unknown");
        }

        if (device->product)
        {
            strncpy(usb_devices_array[index].product, device->product,
                    MAX_STRING_LENGTH - 1);
            usb_devices_array[index].product[MAX_STRING_LENGTH - 1] = '\0';
        }
        else
        {
            strcpy(usb_devices_array[index].product, "Unknown");
        }

        if (device->serial)
        {
            strncpy(usb_devices_array[index].serial, device->serial,
                    MAX_STRING_LENGTH - 1);
            usb_devices_array[index].serial[MAX_STRING_LENGTH - 1] = '\0';
        }
        else
        {
            strcpy(usb_devices_array[index].serial, "Unknown");
        }
        ktime_get_real_ts64(&usb_devices_array[index].added_time);
        usb_devices_array[index].removed_time.tv_sec = 0;
        usb_devices_array[index].removed_time.tv_nsec = 0;
        device_count++;
        mutex_unlock(&rw_mutex);
        return index;
    }
    else
    {
        printk(KERN_DEBUG "Failed to acquire lock.\n");
        return -1;
    }
}

static void remove_device_from_array(int bus_num, int dev_num)
{
    int ret;
    ret = mutex_trylock(&rw_mutex);
    if (ret != 0)
    {
        int index = find_device_in_array(bus_num, dev_num);

        if (index >= 0)
        {
            usb_devices_array[index].is_connected = false;
            ktime_get_real_ts64(&usb_devices_array[index].removed_time);
        }
        mutex_unlock(&rw_mutex);
    }
    else
    {
        printk(KERN_DEBUG "Failed to acquire lock.\n");
    }
}

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
        add_device_to_array(udev);
        break;
    case USB_DEVICE_REMOVE:
        printk(KERN_WARNING PROC_FILENAME ": USB device has been removed.\n");
        print_usb_device_info(udev);
        remove_device_from_array(udev->bus ? udev->bus->busnum : -1, udev->devnum);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block usb_notifier_block = {
    .notifier_call = usb_notifier_callback,
};

// TODO: Don't like this print structure. It is OK for debugging right now.
static int usb_devices_show(struct seq_file *m, void *v)
{
    int i;
    seq_printf(m, "USB Activity Monitor - Device List\n");
    seq_printf(m, "================================\n\n");
    seq_printf(m, "Total Number of Devices: \033[1m%d\033[0m\n\n", device_count);
    for (i = 0; i < device_count; i++)
    {
        seq_printf(m, "\033[1mDevice %d:\n\033[0m", i + 1);
        seq_printf(m, "  Bus: %03d, Device: %03d\n",
                   usb_devices_array[i].bus_num,
                   usb_devices_array[i].dev_num);
        seq_printf(m, "  VID:PID: 0x%04x:0x%04x\n",
                   usb_devices_array[i].vendor_id,
                   usb_devices_array[i].product_id);
        seq_printf(m, "  Manufacturer: %s\n",
                   usb_devices_array[i].manufacturer);
        seq_printf(m, "  Product: %s\n",
                   usb_devices_array[i].product);
        seq_printf(m, "  Serial: %s\n",
                   usb_devices_array[i].serial);
        seq_printf(m, "  Added: %lld.%09lu\n",
                   (long long)usb_devices_array[i].added_time.tv_sec, usb_devices_array[i].added_time.tv_nsec);
        if (!usb_devices_array[i].is_connected)
            seq_printf(m, "  Removed: %lld.%09lu\n",
                       (long long)usb_devices_array[i].removed_time.tv_sec, usb_devices_array[i].removed_time.tv_nsec);
        seq_printf(m, "  Status: %s\n\n",
                   usb_devices_array[i].is_connected ? "\033[32mConnected\033[0m" : "\033[31mDisconnected\033[0m");
    }

    if (device_count == 0)
    {
        seq_printf(m, "No USB devices found.\n");
    }

    return 0;
}

static int usb_devices_open(struct inode *inode, struct file *file)
{
    return single_open(file, usb_devices_show, NULL);
}

static const struct proc_ops usb_activity_monitor_proc_fops = {
    .proc_open = usb_devices_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int usb_for_each_dev_callback(struct usb_device *device, void *ptr)
{
    int err = print_usb_device_info(device);
    if (err)
    {
        return err;
    }
    add_device_to_array(device);
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

static int init_procfs(void)
{
    proc_file = proc_create(PROC_FILENAME, 0644, NULL, &usb_activity_monitor_proc_fops);
    if (NULL == proc_file)
    {
        printk(KERN_ERR PROC_FILENAME ": Could not initialize /proc/%s\n. Aborting..", PROC_FILENAME);
        return -ENOMEM;
    }
    return 0;
}

static int init_usb_devices_array(void)
{
    usb_devices_array = kmalloc_array(MAX_USB_DEVICES, sizeof(struct usb_device_info), GFP_KERNEL);
    if (!usb_devices_array)
    {
        printk(KERN_ERR PROC_FILENAME ": Failed to allocate memory for USB devices array\n");
        return -1;
    }
    memset(usb_devices_array, 0, MAX_USB_DEVICES * sizeof(struct usb_device_info));
    device_count = 0;
    return 0;
}

static int __init usb_activity_monitor_init(void)
{
    printk(KERN_INFO PROC_FILENAME ": Init usb_activity_monitor module\n");
    int err;
    err = init_procfs();
    if (err)
        goto out;
    err = init_usb_devices_array();
    if (err)
        goto out;
    err = list_usb_devices();
    if (err)
        goto out;
    usb_register_notify(&usb_notifier_block);
    return 0;
out:
    if (proc_file)
    {
        proc_remove(proc_file);
        proc_file = NULL;
    }
    if (usb_devices_array)
    {
        kfree(usb_devices_array);
        usb_devices_array = NULL;
    }
    return err;
}

static void __exit usb_activity_monitor_exit(void)
{
    printk(KERN_INFO PROC_FILENAME ": usb_activity_monitor_exit\n");
    if (proc_file)
    {
        proc_remove(proc_file);
        proc_file = NULL;
    }
    if (usb_devices_array)
    {
        kfree(usb_devices_array);
        usb_devices_array = NULL;
    }
    usb_unregister_notify(&usb_notifier_block);
}

module_init(usb_activity_monitor_init);
module_exit(usb_activity_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
