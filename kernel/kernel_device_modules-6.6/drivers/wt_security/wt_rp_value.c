#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/moduleparam.h>
static struct proc_dir_entry *rp_value_node;
static bool parser_paras_from_cmdline(char **value)
{
	int ret = 0;
	struct device_node *node;
	const char *bootparams = NULL;
	char *str = NULL;
    char *endptr = NULL;
    size_t length;

	node = of_find_node_by_path("/chosen");
	if(node) {
		ret = of_property_read_string(node, "bootargs", &bootparams);
		if ((!bootparams) || (ret < 0)) {
			pr_err("secureboot: failed to get bootargs from dts.\n");
			return false;
		}
		str = strstr(bootparams, "wtcmdline.rp_value=");
		if (str) {
			str += strlen("wtcmdline.rp_value=");

            endptr = strchr(str, ' ');

            if (endptr) {
                length = endptr - str;
            } else {
                length = strlen(str);
             }

            *value = kmalloc(length + 1,GFP_KERNEL);
            if (!*value) {
                pr_err("Failed to allocate memory");
                return false;
            }

            strncpy(*value, str, length);
            (*value)[length] = '\0';
            return true;

		}
	}
	pr_err("secureboot: parser paras return error, ret=%d.\n", ret);
	return false;
}
static int rp_value_proc_show(struct seq_file *m, void *v)
{
	bool ret = false;
	char *value = NULL;
	ret = parser_paras_from_cmdline(&value);
	if((value == NULL) || (ret != 1)) {
		pr_err("wt_rp : rp value get error, wt_rp=%s, ret=%d.\n", value, ret);
		return 0;
	}
	seq_printf(m, "%s\n",value);
    kfree(value);
	return 0;
}
static int rp_value_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rp_value_proc_show, NULL);
}
static const struct proc_ops  rp_value_proc_fops = {
	.proc_open		= rp_value_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};
static int __init rp_value_status_init(void)
{
	int ret;
	rp_value_node = proc_create("wt_rp", 0, NULL, &rp_value_proc_fops);
	ret = IS_ERR_OR_NULL(rp_value_node);
	if (ret) {
		pr_err("wt_rp: failed to create proc entry.\n");
		return ret;
	}
	pr_notice("wt_rp: creante node.\n");
	return 0;
}
static void __exit rp_value_status_exit(void)
{
	if (rp_value_node) {
		pr_notice("wt_rp: remove node.\n");
		proc_remove(rp_value_node);
	}
}
module_init(rp_value_status_init);
module_exit(rp_value_status_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WingTech rp value Driver");
MODULE_AUTHOR("WingTech Inc.");
