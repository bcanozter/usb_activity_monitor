# Usb Activity Monitor Kernel Module

This module listens for usb device connection events. Rejects devices that are not in the whitelist. Iterate over already plugged in usb devices on module start. Registers a `struct notifier_block` to receive `USB_DEVICE_ADD` and `USB_DEVICE_REMOVE` events. Maintains a list of usb devices in the proc virtual filesystem for user-space access.

## Features

- Iterate over usb devices on start by using usb_for_each_dev
- Registers a `struct notifier_block` notifier to ensure no usb events are missed.
- Maintain a list of usb devices in the proc virtual filesystem. Includes partial `struct usb_device` information along with connection status.

## Environment Details

- Linux kernel headers: `linux-headers-6.8.0-65-generic`
- GCC: `12.3.0-1ubuntu1~22.04`

## Build

```
make
```

## Load the Module

```
sudo insmod usb_activity_monitor.ko
```

## Usage

Read the list of accepted usb devices.

```
cat /proc/usb_activity_monitor
```

## Example Output

```bash
[ 1317.105430] usb_activity_monitor: Init usb_activity_monitor module
[ 1317.105524] usbcore: registered new interface driver usb_driver
[ 1317.105540] usb_activity_monitor: USB device(VID:PID = 0x1d6b:0x0001) is not in whitelist.
[ 1317.105544] usb_activity_monitor: USB device(VID:PID = 0x1d6b:0x0002) is not in whitelist.
[ 1317.105546] usb_activity_monitor: USB device(VID:PID = 0x80ee:0x0021) is not in whitelist.
[ 1317.105549] usb_activity_monitor: Bus 002 Device 008: VID:PID = 0xffff:0x5678
[ 1317.105553]    Manufacturer: USB
[ 1317.105555]    Product: Disk 2.0
[ 1317.105561]    Serial Number: 1024731271067688855
```

## Unload the Module

```
sudo rmmod usb_activity_monitor.ko
```

---

**Author:** Burak Ozter
