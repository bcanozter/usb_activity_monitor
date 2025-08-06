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
// whitelist of usb devices
static struct usb_device_id whitelist[] = {
    {USB_DEVICE(0xffff, 0x5678)},
    {}};

// Any usb device
static struct usb_device_id usb_id_table[] = {
    {.driver_info = 42},
    {}};

MODULE_DEVICE_TABLE(usb, usb_id_table);

#define MAX_STRING_LENGTH 256

struct usb_device_info
{
    int bus_num;
    int device_num;
    u16 vendor_id;
    u16 product_id;
    char manufacturer[MAX_STRING_LENGTH];
    char product[MAX_STRING_LENGTH];
    char serial[MAX_STRING_LENGTH];
    bool is_connected;
    struct timespec64 added_time;
    struct timespec64 removed_time;
    struct usb_device_info *next;
};

struct usb_device_list
{
    struct usb_device_info *head;
    struct usb_device_info *tail;
    int size;
};

static struct usb_device_list usb_devices = {.head = NULL, .tail = NULL, .size = 0};

// TODO add limit
static void add_device_to_list(struct usb_device_info *new_device)
{
    new_device->next = NULL;
    if (!usb_devices.head)
    {
        usb_devices.head = new_device;
        usb_devices.tail = new_device;
    }
    else
    {
        usb_devices.tail->next = new_device;
        usb_devices.tail = new_device;
    }
    usb_devices.size++;
}

static struct usb_device_info *find_device_in_list(int bus_num, int device_num)
{
    struct usb_device_info *device = usb_devices.head;
    while (device != NULL)
    {
        if (device->bus_num == bus_num && device->device_num == device_num)
        {
            return device;
        }
        device = device->next;
    }
    return NULL;
}

// This only checks whether both idVendor and idProduct is identical.
//  This will change.
static bool is_device_in_whitelist(const struct usb_device *device)
{
    for (int i = 0; i < ARRAY_SIZE(whitelist); i++)
    {
        // printk(KERN_DEBUG "VID:PID = 0x%04x:0x%04x\n", le16_to_cpu(device->descriptor.idVendor), le16_to_cpu(device->descriptor.idProduct));
        if ((whitelist[i].idVendor == device->descriptor.idVendor) && (whitelist[i].idProduct == device->descriptor.idProduct))
        {
            return true;
        }
    }
    return false;
}

static void add_usb_device(struct usb_device *device)
{
    struct usb_device_info *new_device = kmalloc(sizeof(struct usb_device_info), GFP_KERNEL);
    if (!new_device)
    {
        return;
    }
    new_device->bus_num = device->bus ? device->bus->busnum : -1;
    new_device->device_num = device->devnum;
    new_device->vendor_id = le16_to_cpu(device->descriptor.idVendor);
    new_device->product_id = le16_to_cpu(device->descriptor.idProduct);
    new_device->is_connected = true;
    if (device->manufacturer)
    {
        strncpy(new_device->manufacturer, device->manufacturer, MAX_STRING_LENGTH - 1);
    }
    else
    {
        strcpy(new_device->manufacturer, "Unknown");
    }
    if (device->product)
    {
        strncpy(new_device->product, device->product, MAX_STRING_LENGTH - 1);
    }
    else
    {
        strcpy(new_device->product, "Unknown");
    }
    if (device->serial)
    {
        strncpy(new_device->serial, device->serial, MAX_STRING_LENGTH - 1);
    }
    else
    {
        strcpy(new_device->serial, "Unknown");
    }
    ktime_get_real_ts64(&new_device->added_time);
    new_device->removed_time.tv_sec = 0;
    new_device->removed_time.tv_nsec = 0;
    add_device_to_list(new_device);
}

// Mark device as disconnected and set removal time
static void disconnect_usb_device(int bus_num, int device_num)
{
    struct usb_device_info *device = find_device_in_list(bus_num, device_num);
    if (device && device->is_connected)
    {
        device->is_connected = false;
        ktime_get_real_ts64(&device->removed_time);
    }
}

static void print_usb_device_info(struct usb_device *device)
{

    printk(KERN_INFO PROC_FILENAME ": Bus %03d Device %03d: VID:PID = 0x%04x:0x%04x\n",
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
}

static int usb_notifier_callback(struct notifier_block *self,
                                 unsigned long action, void *dev)
{
    struct usb_device *device = dev;
    if (!is_device_in_whitelist(device))
    {
        printk(KERN_WARNING PROC_FILENAME ": USB device(VID:PID = 0x%04x:0x%04x) is not in whitelist.\n", le16_to_cpu(device->descriptor.idVendor), le16_to_cpu(device->descriptor.idProduct));
        return -1;
    }
    switch (action)
    {
    case USB_DEVICE_ADD:
        printk(KERN_WARNING PROC_FILENAME ": USB device has been added.\n");
        print_usb_device_info(device);
        add_usb_device(device);
        break;
    case USB_DEVICE_REMOVE:
        printk(KERN_WARNING PROC_FILENAME ": USB device has been removed.\n");
        print_usb_device_info(device);
        disconnect_usb_device(device->bus ? device->bus->busnum : -1, device->devnum);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block usb_notifier_block = {
    .notifier_call = usb_notifier_callback,
};

// Show devices in /proc
static int usb_devices_show(struct seq_file *m, void *v)
{
    struct usb_device_info *curr = usb_devices.head;
    seq_printf(m, "USB Activity Monitor - Device List\n");
    seq_printf(m, "================================\n\n");
    seq_printf(m, "Total Number of Devices: \033[1m%d\033[0m\n\n", usb_devices.size);
    if (!curr)
    {
        seq_printf(m, "No USB devices found.\n");
        return 0;
    }
    int i = 1;
    while (curr)
    {
        seq_printf(m, "\033[1mDevice %d:\n\033[0m", i);
        seq_printf(m, "  Bus: %03d, Device: %03d\n", curr->bus_num, curr->device_num);
        seq_printf(m, "  VID:PID: 0x%04x:0x%04x\n", curr->vendor_id, curr->product_id);
        seq_printf(m, "  Manufacturer: %s\n", curr->manufacturer);
        seq_printf(m, "  Product: %s\n", curr->product);
        seq_printf(m, "  Serial: %s\n", curr->serial);
        seq_printf(m, "  Added: %lld.%09lu\n", (long long)curr->added_time.tv_sec, curr->added_time.tv_nsec);
        if (!curr->is_connected)
            seq_printf(m, "  Removed: %lld.%09lu\n", (long long)curr->removed_time.tv_sec, curr->removed_time.tv_nsec);
        seq_printf(m, "  Status: %s\n\n", curr->is_connected ? "\033[32mConnected\033[0m" : "\033[31mDisconnected\033[0m");
        curr = curr->next;
        i++;
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
    if (device)
    {
        if (!is_device_in_whitelist(device))
        {
            printk(KERN_WARNING PROC_FILENAME ": USB device(VID:PID = 0x%04x:0x%04x) is not in whitelist.\n", le16_to_cpu(device->descriptor.idVendor), le16_to_cpu(device->descriptor.idProduct));
        }
        else
        {
            print_usb_device_info(device);
            add_usb_device(device);
        }
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

static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *device = interface_to_usbdev(interface);
    if (!is_device_in_whitelist(device))
    {
        printk(KERN_WARNING PROC_FILENAME ": USB device(VID:PID = 0x%04x:0x%04x) is not in whitelist.\n", le16_to_cpu(device->descriptor.idVendor), le16_to_cpu(device->descriptor.idProduct));
        return -ENODEV;
    }
    return 0;
}

static void usb_disconnect(struct usb_interface *interface)
{
    return;
}

static struct usb_driver _usb_driver =
    {
        .name = "usb_driver",
        .id_table = usb_id_table,
        .probe = usb_probe,
        .disconnect = usb_disconnect,
};

static int __init usb_activity_monitor_init(void)
{
    printk(KERN_INFO PROC_FILENAME ": Init usb_activity_monitor module\n");
    int err;
    err = usb_register(&_usb_driver);
    if (err)
    {
        printk(KERN_ERR PROC_FILENAME ": Failed to register usb_id_table.\n");
        goto out;
    }
    err = init_procfs();
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
    usb_deregister(&_usb_driver);
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
    usb_unregister_notify(&usb_notifier_block);
    usb_deregister(&_usb_driver);
}

module_init(usb_activity_monitor_init);
module_exit(usb_activity_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
