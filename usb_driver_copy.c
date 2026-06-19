#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>

static struct usb_device *device;

/* USB Device ID Table */
static struct usb_device_id skel_table[] = {
    { USB_DEVICE(0x0, 0x0) }, 
    { } 
};

MODULE_DEVICE_TABLE(usb, skel_table);

/* Probe Function */
static int skel_probe(struct usb_interface *interface,
                      const struct usb_device_id *id)
{
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;

    device = interface_to_usbdev(interface);

    iface_desc = interface->cur_altsetting;

    printk(KERN_INFO "USB Device Probed: (%04X:%04X)\n",
           id->idVendor, id->idProduct);

    printk(KERN_INFO "Interface Number: %d\n",
           iface_desc->desc.bInterfaceNumber);

    printk(KERN_INFO "Number of Endpoints: %d\n",
           iface_desc->desc.bNumEndpoints);

    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {

        endpoint = &iface_desc->endpoint[i].desc;

        printk(KERN_INFO "Endpoint Address: 0x%02X\n",
               endpoint->bEndpointAddress);

        printk(KERN_INFO "Attributes: 0x%02X\n",
               endpoint->bmAttributes);

        printk(KERN_INFO "Max Packet Size: %d\n",
               endpoint->wMaxPacketSize);
    }

    return 0;
}

/* Disconnect Function */
static void skel_disconnect(struct usb_interface *interface)
{
    printk(KERN_INFO "USB Device Removed\n");
}

/* USB Driver Structure */
static struct usb_driver skel_driver = {
    .name = "usb_skel_driver",
    .probe = skel_probe,
    .disconnect = skel_disconnect,
    .id_table = skel_table,
};

/* Module Init */
static int __init usb_skel_init(void)
{
    int result;

    result = usb_register(&skel_driver);

    if (result < 0) {
        pr_err("USB driver registration failed\n");
        return result;
    }

    printk(KERN_INFO "USB Skeleton Driver Initialized\n");

    return 0;
}

/* Module Exit */
static void __exit usb_skel_exit(void)
{
    usb_deregister(&skel_driver);

    printk(KERN_INFO "USB Skeleton Driver Exited\n");
}

module_init(usb_skel_init);
module_exit(usb_skel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthikeya");
MODULE_DESCRIPTION(" USB Skeleton Driver");

