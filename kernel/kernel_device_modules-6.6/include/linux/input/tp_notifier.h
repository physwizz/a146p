#ifndef TP_NOTIFIER_H
#define TP_NOTIFIER_H

#define USB_PLUG_IN 1
#define USB_PLUG_OUT 0
#define EARPHONE_PLUG_IN 3
#define EARPHONE_PLUG_OUT 2

int usb_register_notifier_chain_for_tp(struct notifier_block *nb);
int usb_unregister_notifier_chain_for_tp(struct notifier_block *nb);
int usb_notifier_call_chain_for_tp(unsigned long val, void *v);

#endif