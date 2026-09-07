#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/input/tp_notifier.h>

static BLOCKING_NOTIFIER_HEAD(usb_tp_notifier_list);

#if IS_ENABLED(CONFIG_WT_PROJECT_S96818AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96818BA1)
bool n28_gesture_status = 0;
EXPORT_SYMBOL(n28_gesture_status);
#endif

#if IS_ENABLED(CONFIG_WT_PROJECT_S96901AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901WA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96902AA1)
bool w2_gesture_status = 0;
EXPORT_SYMBOL(w2_gesture_status);
#endif

int usb_register_notifier_chain_for_tp(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&usb_tp_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(usb_register_notifier_chain_for_tp);

int usb_unregister_notifier_chain_for_tp(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&usb_tp_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(usb_unregister_notifier_chain_for_tp);

int usb_notifier_call_chain_for_tp(unsigned long val, void *v)
{
    return blocking_notifier_call_chain(&usb_tp_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(usb_notifier_call_chain_for_tp);

MODULE_LICENSE("GPL v2");