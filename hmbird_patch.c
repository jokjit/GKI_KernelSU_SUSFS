#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/string.h>

/*
 * hmbird_patch_init() 在 very early boot 阶段执行（early_initcall）
 * 主要作用：
 *   1) 找到设备树节点 /soc/oplus,hmbird/version_type
 *   2) 读取其 type 属性
 *   3) 如果 type == "HMBIRD_OGKI"，则强制替换为 "HMBIRD_GKI"
 *
 * 这属于“设备树运行时修补”逻辑，常用于兼容厂商判断分支/内核类型的场景。
 */
static int __init hmbird_patch_init(void)
{
    /* 设备树节点句柄 */
    struct device_node *ver_np;
    /* 指向 type 属性字符串 */
    const char *type;
    int ret;

    /* 按绝对路径查找目标节点 */
    ver_np = of_find_node_by_path("/soc/oplus,hmbird/version_type");
    if (!ver_np)
    {
        pr_info("hmbird_patch: version_type node not found\n");
        /* 返回 0 代表“不阻止启动流程”，即使补丁未生效也不中断系统 */
        return 0;
    }

    /* 读取节点中的 type 字符串属性 */
    ret = of_property_read_string(ver_np, "type", &type);
    if (ret)
    {
        pr_info("hmbird_patch: type property not found\n");
        /* 释放节点引用，避免引用计数泄漏 */
        of_node_put(ver_np);
        return 0;
    }

    /* 仅当原值是 HMBIRD_OGKI 时才执行替换 */
    if (strcmp(type, "HMBIRD_OGKI"))
    {
        of_node_put(ver_np);
        return 0;
    }

    /* 获取原属性结构体，后续复制一份并替换 value */
    struct property *prop = of_find_property(ver_np, "type", NULL);
    if (prop)
    {
        /* 为新属性结构体分配内存 */
        struct property *new_prop = kmalloc(sizeof(*prop), GFP_KERNEL);
        if (!new_prop)
        {
            pr_info("hmbird_patch: kmalloc for new_prop failed\n");
            of_node_put(ver_np);
            return 0;
        }

        /* 先拷贝旧属性结构，再单独替换 value 与 length */
        memcpy(new_prop, prop, sizeof(*prop));
        new_prop->value = kmalloc(strlen("HMBIRD_GKI") + 1, GFP_KERNEL);
        if (!new_prop->value)
        {
            pr_info("hmbird_patch: kmalloc for new_prop->value failed\n");
            kfree(new_prop);
            of_node_put(ver_np);
            return 0;
        }
        strcpy(new_prop->value, "HMBIRD_GKI");
        new_prop->length = strlen("HMBIRD_GKI") + 1;

        /* 先移除旧属性，再添加新属性 */
        if (of_remove_property(ver_np, prop) != 0)
        {
            pr_info("hmbird_patch: of_remove_property failed\n");
            return 0;
        }
        if (of_add_property(ver_np, new_prop) != 0)
        {
            pr_info("hmbird_patch: of_add_property failed\n");
            return 0;
        }
        pr_info("hmbird_patch: success from HMBIRD_OGKI to HMBIRD_GKI\n");
    }
    else
    {
        pr_info("hmbird_patch: type property structure not found\n");
    }
    of_node_put(ver_np);
    return 0;
}

/* 提前在启动早期执行，确保后续组件读到的是修补后的 type 值 */
early_initcall(hmbird_patch_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("reigadegr");
MODULE_DESCRIPTION("Forcefully convert HMBIRD_OGKI to HMBIRD_GKI.");