/*
 * This software is Copyright by the Board of Trustees of Michigan
 * State University (c) Copyright 2017.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/interrupt.h>
#include <linux/uio.h>
#include <linux/uio_driver.h>

#define DRV_NAME "frib_bpm"
#define DRV_VERSION "0"

struct bpm_priv {
    struct uio_info uio;
    struct pci_dev *pci;
    unsigned block_size; /* size in bytes */
    u32 *block;
};

#define CAPTURE_BASE 0
#define CAPTURE_SIZE 16

static
irqreturn_t bpm_handler(int irq, struct uio_info *dev_info)
{
    struct bpm_priv *priv = container_of(dev_info, struct bpm_priv, uio);
    unsigned i;
    u32 *addr = (u32*)(CAPTURE_BASE+(char*)priv->uio.mem[0].internal_addr);

    for(i=0; i<CAPTURE_SIZE; i++, addr++) {
        ACCESS_ONCE(priv->block[i]) = ioread32(addr);
    }

    wmb();

    //TODO: need to invalidate cpu cache or otherwise additional sync. with userspace?

    return IRQ_HANDLED;
}

#define CHECK(LABEL, MSG) if(ret) { dev_err(&priv->pci->dev, MSG); goto err_ ## LABEL; }

static
int bpm_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int ret;
    struct bpm_priv *priv;

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if(!priv) return -ENOMEM;

    priv->pci = dev;
    priv->uio.name = DRV_NAME;
    priv->uio.version = DRV_VERSION;

    ret = pci_enable_device(dev);
    CHECK(enable_device, "Can't enable PCI Device");

    if (pci_request_regions(dev, DRV_NAME))  ret=-ENODEV;
    CHECK(request_regions, "Can't request regions");

    pci_set_master(dev);

    priv->uio.mem[0].name = "BAR0";
    priv->uio.mem[0].addr = pci_resource_start(dev, 0);
    priv->uio.mem[0].size = pci_resource_len(dev, 0);
    priv->uio.mem[0].internal_addr =pci_ioremap_bar(dev, 0);
    priv->uio.mem[0].memtype = UIO_MEM_PHYS;

    if(!priv->uio.mem[0].internal_addr)  ret=-ENODEV;
    CHECK(ioremap, "Failed to map BAR0");

    //TODO: inspect to find block size
    priv->block_size = CAPTURE_BASE;

    if(priv->block_size==0) ret=-ENOMEM;
    CHECK(block_size, "Zero block size");

    // round up to PAGE_SIZE
    // simplify mmap() by having only whole page(s)
    priv->block_size = ((priv->block_size-1u)|PAGE_MASK)+1u;

    priv->block = kzalloc(priv->block_size, GFP_KERNEL);
    if(!priv->block) ret=-ENOMEM;
    CHECK(block_alloc, "Failed to allocate for capture block");

    priv->uio.mem[1].name = "CAP";
    priv->uio.mem[1].addr = (phys_addr_t)priv->block;
    priv->uio.mem[1].size = priv->block_size;
    priv->uio.mem[1].memtype = UIO_MEM_LOGICAL;

    ret = pci_enable_msi(dev);
    CHECK(enable_msi, "Can't enable MSI");

    priv->uio.irq = dev->irq;
    priv->uio.handler = bpm_handler;

    pci_set_drvdata(dev, priv);

    ret = uio_register_device(&dev->dev, &priv->uio);
    CHECK(unregister, "Can't create UIO");

    return 0;

    //uio_unregister_device(&priv->uio);
    //pci_set_drvdata(dev, NULL);
err_unregister:
    pci_disable_msi(dev);
err_block_alloc:
    kfree(priv->block);
err_enable_msi:
    iounmap(priv->uio.mem[0].internal_addr);
err_block_size:
err_ioremap:
    pci_release_regions(dev);
err_request_regions:
    pci_disable_device(dev);
err_enable_device:
    kfree(priv);
    return ret;
}

#undef CHECK

static
void bpm_remove(struct pci_dev *dev)
{
    struct bpm_priv *priv = pci_get_drvdata(dev);
    if(!priv) return;

    uio_unregister_device(&priv->uio);
    pci_set_drvdata(dev, NULL);

    pci_disable_msi(dev);
    kfree(priv->block);
    iounmap(priv->uio.mem[0].internal_addr);
    pci_release_regions(dev);
    pci_disable_device(dev);
    kfree(priv);

}

static const struct pci_device_id bpm_ids[] = {
    {
        .vendor = PCI_VENDOR_ID_XILINX,
        .device = 0xfbad, /* FRIB FGPDB as BPM */
    },
    { 0, }
};

static struct pci_driver bpm_driver = {
    .name = DRV_NAME,
    .id_table = bpm_ids,
    .probe = bpm_probe,
    .remove = bpm_remove,
};

static
int __init bpm_init(void)
{
    return pci_register_driver(&bpm_driver);
}

static
void __exit bpm_exit(void)
{
    pci_unregister_driver(&bpm_driver);
}

module_init(bpm_init);
module_exit(bpm_exit);

MODULE_DEVICE_TABLE(pci, bpm_ids);

MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("mdavidsaver@gmail.com");
MODULE_DESCRIPTION("Stub driver for FRIB BPM");
