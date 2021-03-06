/******************************************************************************
**
** FILE NAME    : ifxmips_eth_framework.c
** PROJECT      : UEIP
** MODULES      : ETH
**
** DATE         : 2 Nov 2010
** AUTHOR       : Xu Liang
** DESCRIPTION  : IFX ETH driver framework source file
** COPYRIGHT    :              Copyright (c) 2009
**                          Lantiq Deutschland GmbH
**                   Am Campeon 3; 85579 Neubiberg, Germany
**
**   For licensing information, see the file 'LICENSE' in the root folder of
**   this software module.
**
** HISTORY
** $Date        $Author         $Comment
** 02 NOV 2010  Xu Liang        Init Version
*******************************************************************************/



/*
 * ####################################
 *              Head File
 * ####################################
 */

/*
 *  Common Head File
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>

/*
 *  Chip Specific Head File
 */
#include <asm/ifx/ifx_types.h>
#include "ifxmips_eth_framework.h"



/*
 * ####################################
 *              Definition
 * ####################################
 */



/*
 * ####################################
 *             Declaration
 * ####################################
 */

/*
 *  Network Operations
 */
static int eth_open(struct net_device *);
static int eth_stop(struct net_device *);
static int eth_start_xmit(struct sk_buff *, struct net_device *);
static int eth_ioctl(struct net_device *, struct ifreq *, int);
struct net_device_stats * eth_get_stats(struct net_device *);
static void eth_tx_timeout(struct net_device *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
  static int eth_poll(struct net_device *, int *);
#else
  static int eth_poll(struct napi_struct *, int);
#endif

static struct net_device *eth_alloc_netdev(int, const char *);



/*
 * ####################################
 *            Local Variable
 * ####################################
 */

static DEFINE_SPINLOCK(g_netdev_list_lock);
static LIST_HEAD(g_netdev_list);



/*
 * ####################################
 *            Local Function
 * ####################################
 */

static int eth_open(struct net_device *dev)
{
    int ret = 0;
    struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if ( priv->poll != NULL )
        napi_enable(&priv->napi);
#endif
    if ( priv->open != NULL )
        ret = priv->open(dev);
    netif_start_queue(dev);

    return ret;
}

static int eth_stop(struct net_device *dev)
{
    struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);

    if ( priv->stop != NULL )
        priv->stop(dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if ( priv->poll != NULL )
        napi_disable(&priv->napi);
#endif
    netif_stop_queue(dev);

    return 0;
}

static int eth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);

    priv->stats_bak.tx_dropped++;
    dev_kfree_skb_any(skb);

    return NETDEV_TX_OK;
}

static int eth_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    return ENOIOCTLCMD;
}

struct net_device_stats * eth_get_stats(struct net_device *dev)
{
    struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);

    if ( test_bit(ETHFW_PRIV_DATA_FLAG_REG, &priv->flags) && priv->get_stats != NULL ) {
        struct net_device_stats *stats = priv->get_stats(dev);
        if ( stats != NULL )
            priv->stats = *stats;
    }
    else
        priv->stats = priv->stats_bak;

    return &priv->stats;
}

static void eth_tx_timeout(struct net_device *dev)
{
    netif_wake_queue(dev);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
static int eth_poll(struct net_device *dev, int *quota)
{
    struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);
    unsigned int work_to_do = min(dev->quota, *quota);
    unsigned int work_done = 0;
    ifx_eth_fw_poll_ret_t poll_ret;

    poll_ret = priv->poll(dev, work_to_do, &work_done);

    *quota -= work_done;
    dev->quota -= work_done;

    if ( poll_ret == IFX_ETH_FW_POLL_CONTINUE && !netif_running(dev) ) {
        netif_rx_complete(dev);
        return IFX_ETH_FW_POLL_COMPLETE;
    }

    return poll_ret;
}
#else
static int eth_poll(struct napi_struct *napi, int budget)
{
    struct net_device *dev = napi->dev;
    struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);
    unsigned int work_done = 0;
    ifx_eth_fw_poll_ret_t poll_ret;

    poll_ret = priv->poll(dev, budget, &work_done);
    if ( poll_ret == IFX_ETH_FW_POLL_CONTINUE ) {
        if ( netif_running(dev) )
            return work_done;
        napi_complete(napi);
    }
    return 0;   //  complete, to workaround improper "list_move_tail(&n->poll_list, list);" in "net_rx_action"
}
#endif

static struct net_device *eth_alloc_netdev(int sizeof_priv, const char *name)
{
    unsigned long sys_flags;
    struct list_head *list;
    struct ifx_eth_fw_priv_data *priv;
    struct net_device *dev;

    spin_lock_irqsave(&g_netdev_list_lock, sys_flags);
    list_for_each(list, &g_netdev_list) {
        priv = list_entry(list, struct ifx_eth_fw_priv_data, list);
        if ( strcmp(priv->dev->name, name) == 0 ) {
            spin_unlock_irqrestore(&g_netdev_list_lock, sys_flags);
            if ( test_and_set_bit(ETHFW_PRIV_DATA_FLAG_ALLOC, &priv->flags) )
                return NULL;
            if ( priv->sizeof_priv >= sizeof_priv ) {
                if ( priv->malloc_size > 0 ) {
                    kfree(priv->priv);
                    priv->malloc_size = 0;
                }
                priv->priv = (char *)priv + sizeof(struct ifx_eth_fw_priv_data);
                memset(priv->priv, 0, sizeof_priv);
            }
            else if ( priv->malloc_size < sizeof_priv ) {
                if ( priv->malloc_size > 0 ) {
                    kfree(priv->priv);
                    priv->malloc_size = 0;
                }
                priv->priv = kzalloc(sizeof_priv, GFP_KERNEL);
                if ( priv->priv == NULL ) {
                    clear_bit(ETHFW_PRIV_DATA_FLAG_ALLOC, &priv->flags);
                    return NULL;
                }
                priv->malloc_size = sizeof_priv;
            }
            return priv->dev;
        }
    }
    spin_unlock_irqrestore(&g_netdev_list_lock, sys_flags);

    dev = alloc_netdev(sizeof(struct ifx_eth_fw_priv_data) + sizeof_priv, name, ether_setup);
    if ( dev != NULL ) {
        priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);
        priv->dev           = dev;
        priv->priv          = (char *)priv + sizeof(struct ifx_eth_fw_priv_data);
        priv->sizeof_priv   = sizeof_priv;
        set_bit(ETHFW_PRIV_DATA_FLAG_ALLOC, &priv->flags);

        spin_lock_irqsave(&g_netdev_list_lock, sys_flags);
        list_add_tail(&priv->list, &g_netdev_list);
        spin_unlock_irqrestore(&g_netdev_list_lock, sys_flags);
    }
    return dev;
}



/*
 * ####################################
 *           Global Function
 * ####################################
 */

void *ifx_eth_fw_netdev_priv(struct net_device *dev)
{
    return dev == NULL ? NULL : ((struct ifx_eth_fw_priv_data *)netdev_priv(dev))->priv;
}
EXPORT_SYMBOL(ifx_eth_fw_netdev_priv);

struct net_device *ifx_eth_fw_alloc_netdev(int sizeof_priv, const char *name, struct ifx_eth_fw_netdev_ops *ops)
{
    struct net_device *dev;

    if ( ops == NULL )
        return NULL;

    if ( name == NULL )
        name = "eth%d";

    dev = eth_alloc_netdev(sizeof_priv, name);
    if ( dev != NULL ) {
        struct ifx_eth_fw_priv_data *priv = netdev_priv(dev);

        if ( test_bit(ETHFW_PRIV_DATA_FLAG_REG_NETDEV, &priv->flags) && ops->init != NULL )
            ops->init(dev);

        priv->open      = ops->open;
        priv->stop      = ops->stop;
        priv->get_stats = ops->get_stats;

        dev->watchdog_timeo = ops->watchdog_timeo;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
        dev->init               = ops->init;
        dev->uninit             = ops->uninit;
        dev->open               = eth_open;
        dev->stop               = eth_stop;
        dev->hard_start_xmit    = ops->start_xmit;
        dev->set_multicast_list = ops->set_multicast_list;
        if ( ops->set_mac_address != NULL )
            dev->set_mac_address= ops->set_mac_address;
        dev->do_ioctl           = ops->do_ioctl;
        dev->set_config         = ops->set_config;
        if ( ops->change_mtu != NULL )
            dev->change_mtu     = ops->change_mtu;
        dev->neigh_setup        = ops->neigh_setup;
        dev->get_stats          = eth_get_stats;
  #ifdef CONFIG_NET_POLL_CONTROLLER
        dev->poll_controller    = ops->poll_controller;
  #endif
        dev->tx_timeout         = ops->tx_timeout;

        if ( ops->poll && ops->weight >= 0 ) {
            priv->poll = ops->poll;
            dev->poll           = eth_poll;
            dev->weight         = ops->weight;
        }
#else
        priv->netdev_ops.ndo_init               = ops->init;
        priv->netdev_ops.ndo_uninit             = ops->uninit;
        priv->netdev_ops.ndo_open               = eth_open;
        priv->netdev_ops.ndo_stop               = eth_stop;
        priv->netdev_ops.ndo_start_xmit         = ops->start_xmit;
        priv->netdev_ops.ndo_set_multicast_list = ops->set_multicast_list;
        priv->netdev_ops.ndo_set_mac_address    = ops->set_mac_address == NULL ? eth_mac_addr : ops->set_mac_address;
        priv->netdev_ops.ndo_validate_addr      = eth_validate_addr;
        priv->netdev_ops.ndo_do_ioctl           = ops->do_ioctl;
        priv->netdev_ops.ndo_set_config         = ops->set_config;
        priv->netdev_ops.ndo_change_mtu         = ops->change_mtu == NULL ? eth_change_mtu : ops->change_mtu;
        priv->netdev_ops.ndo_neigh_setup        = ops->neigh_setup;
        priv->netdev_ops.ndo_get_stats          = eth_get_stats;
  #ifdef CONFIG_NET_POLL_CONTROLLER
        priv->netdev_ops.ndo_poll_controller    = ops->poll_controller;
  #endif
        priv->netdev_ops.ndo_tx_timeout         = ops->tx_timeout;

        if ( ops->poll && ops->weight >= 0 ) {
            priv->poll = ops->poll;
            netif_napi_add(dev, &priv->napi, eth_poll, ops->weight);
        }

        dev->netdev_ops = &priv->netdev_ops;
#endif
    }

    return dev;
}
EXPORT_SYMBOL(ifx_eth_fw_alloc_netdev);

void ifx_eth_fw_free_netdev(struct net_device *dev, int force)
{
    struct ifx_eth_fw_priv_data *priv;

    if ( dev == NULL )
        return;

    priv = netdev_priv(dev);

    if ( !test_bit(ETHFW_PRIV_DATA_FLAG_ALLOC, &priv->flags) )
        return;

    ifx_eth_fw_unregister_netdev(dev, force);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if ( priv->poll != NULL )
        netif_napi_del(&priv->napi);
#endif
    priv->poll = NULL;
    clear_bit(ETHFW_PRIV_DATA_FLAG_ALLOC, &priv->flags);

    if ( force ) {
        unsigned long sys_flags;

        spin_lock_irqsave(&g_netdev_list_lock, sys_flags);
        list_del(&priv->list);
        spin_unlock_irqrestore(&g_netdev_list_lock, sys_flags);

        if ( priv->malloc_size > 0 )
            kfree(priv->priv);

        free_netdev(dev);
    }
}
EXPORT_SYMBOL(ifx_eth_fw_free_netdev);

int ifx_eth_fw_register_netdev(struct net_device *dev)
{
    struct ifx_eth_fw_priv_data *priv;
    int ret;

    if ( dev == NULL )
        return -EINVAL;

    priv = netdev_priv(dev);

    if ( test_and_set_bit(ETHFW_PRIV_DATA_FLAG_REG, &priv->flags) )
        return -ENFILE; //  in use

    if ( test_and_set_bit(ETHFW_PRIV_DATA_FLAG_REG_NETDEV, &priv->flags) ) {
        struct net_device_stats *stats = priv->get_stats(dev);
        if ( stats != NULL )
            *stats = priv->stats_bak;
        if ( netif_running(dev) )
            eth_open(dev);
        return 0;
    }

    ret = register_netdev(dev);
    if ( ret != 0 )
        clear_bit(ETHFW_PRIV_DATA_FLAG_REG_NETDEV, &priv->flags);

    return ret;
}
EXPORT_SYMBOL(ifx_eth_fw_register_netdev);

void ifx_eth_fw_unregister_netdev(struct net_device *dev, int force)
{
    struct ifx_eth_fw_priv_data *priv;
    struct net_device_stats *stats;

    if ( dev == NULL )
        return;

    priv = netdev_priv(dev);

    if ( force ) {
        if ( test_bit(ETHFW_PRIV_DATA_FLAG_REG_NETDEV, &priv->flags) ) {
            unregister_netdev(dev);
            clear_bit(ETHFW_PRIV_DATA_FLAG_REG_NETDEV, &priv->flags);
        }
        clear_bit(ETHFW_PRIV_DATA_FLAG_REG, &priv->flags);
        return;
    }

    if ( !test_bit(ETHFW_PRIV_DATA_FLAG_REG, &priv->flags) )
        return;

    if ( netif_running(dev) ) {
        priv->stop(dev);
        msleep(1);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
        if ( priv->poll != NULL && test_bit(NAPI_STATE_SCHED, &priv->napi.state) )
            napi_complete(&priv->napi);
#endif
    }
    stats = priv->get_stats(dev);
    if ( stats != NULL )
        priv->stats_bak = *stats;
    priv->get_stats = NULL;
    priv->open      = NULL;
    priv->stop      = NULL;
    SET_ETHTOOL_OPS(dev, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    if ( dev->uninit != NULL )
        dev->uninit(dev);
    dev->init               = NULL;
    dev->uninit             = NULL;
    dev->hard_start_xmit    = eth_start_xmit;
    dev->set_multicast_list = NULL;
    dev->set_mac_address    = NULL;
    dev->do_ioctl           = eth_ioctl;
    dev->set_config         = NULL;
    dev->change_mtu         = NULL;
    dev->neigh_setup        = NULL;
  #ifdef CONFIG_NET_POLL_CONTROLLER
    dev->poll_controller    = NULL;
  #endif
    dev->tx_timeout         = eth_tx_timeout;
#else
    if ( priv->netdev_ops.ndo_uninit != NULL )
        priv->netdev_ops.ndo_uninit(dev);
    priv->netdev_ops.ndo_init               = NULL;
    priv->netdev_ops.ndo_uninit             = NULL;
    priv->netdev_ops.ndo_start_xmit         = eth_start_xmit;
    priv->netdev_ops.ndo_set_multicast_list = NULL;
    priv->netdev_ops.ndo_set_mac_address    = eth_mac_addr;
    priv->netdev_ops.ndo_do_ioctl           = eth_ioctl;
    priv->netdev_ops.ndo_set_config         = NULL;
    priv->netdev_ops.ndo_change_mtu         = eth_change_mtu;
    priv->netdev_ops.ndo_neigh_setup        = NULL;
  #ifdef CONFIG_NET_POLL_CONTROLLER
    priv->netdev_ops.ndo_poll_controller    = NULL;
  #endif
    priv->netdev_ops.ndo_tx_timeout         = eth_tx_timeout;
#endif
    clear_bit(ETHFW_PRIV_DATA_FLAG_REG, &priv->flags);
}
EXPORT_SYMBOL(ifx_eth_fw_unregister_netdev);

int ifx_eth_fw_poll_schedule(struct net_device *dev)
{
    if ( dev == NULL )
        return -EINVAL;
    else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
        netif_rx_schedule(dev);
#else
        struct ifx_eth_fw_priv_data *priv;

        priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);
        napi_schedule(&priv->napi);
#endif
        return 0;
    }
}
EXPORT_SYMBOL(ifx_eth_fw_poll_schedule);

int ifx_eth_fw_poll_complete(struct net_device *dev)
{
    if ( dev == NULL )
        return -EINVAL;
    else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
        if ( test_bit(__LINK_STATE_RX_SCHED, &dev->state) )
            netif_rx_complete(dev);
#else
        struct ifx_eth_fw_priv_data *priv = (struct ifx_eth_fw_priv_data *)netdev_priv(dev);

        if ( test_bit(NAPI_STATE_SCHED, &priv->napi.state) )
            napi_complete(&priv->napi);
#endif
        return 0;
    }
}
EXPORT_SYMBOL(ifx_eth_fw_poll_complete);



/*
 * ####################################
 *           Init/Cleanup API
 * ####################################
 */

static int __devinit ifx_eth_fw_init(void)
{
    return 0;
}

static void __exit ifx_eth_fw_exit(void)
{
}

module_init(ifx_eth_fw_init);
module_exit(ifx_eth_fw_exit);
