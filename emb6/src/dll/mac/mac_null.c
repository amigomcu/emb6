/**
 * @file    mac_null.c
 * @date    16.10.2015
 * @author  PN
 */


/*
********************************************************************************
*                                   INCLUDES
********************************************************************************
*/
#include "emb6.h"
#include "packetbuf.h"
#include "linkaddr.h"

/*
********************************************************************************
*                          LOCAL FUNCTION DECLARATIONS
********************************************************************************
*/
static void MAC_Init(void *p_netstk, e_nsErr_t *p_err);
static void MAC_On(e_nsErr_t *p_err);
static void MAC_Off(e_nsErr_t *p_err);
static void MAC_Send(uint8_t *p_data, uint16_t len, e_nsErr_t *p_err);
static void MAC_Recv(uint8_t *p_data, uint16_t len, e_nsErr_t *p_err);
static void MAC_IOCtrl(e_nsIocCmd_t cmd, void *p_val, e_nsErr_t *p_err);


/*
********************************************************************************
*                               LOCAL VARIABLES
********************************************************************************
*/
static s_ns_t *MAC_Netstk;

/*
********************************************************************************
*                               GLOBAL VARIABLES
********************************************************************************
*/
const s_nsMAC_t MACDrvNull =
{
   "MAC NULL",
    MAC_Init,
    MAC_On,
    MAC_Off,
    MAC_Send,
    MAC_Recv,
    MAC_IOCtrl
};

extern uip_lladdr_t uip_lladdr;

/*
********************************************************************************
*                           LOCAL FUNCTION DEFINITIONS
********************************************************************************
*/
static void MAC_Init(void *p_netstk, e_nsErr_t *p_err)
{
#if NETSTK_CFG_ARG_CHK_EN
    if (p_netstk == NULL) {
        *p_err = NETSTK_ERR_INVALID_ARGUMENT;
        return;
    }
#endif

    /* store pointer to netstack structure */
    MAC_Netstk = p_netstk;

    /* configure stack MAC address */
    memcpy(&uip_lladdr.addr, &mac_phy_config.mac_address, 8);
    linkaddr_set_node_addr((linkaddr_t *)mac_phy_config.mac_address);

    /* set returned error code */
    *p_err = NETSTK_ERR_NONE;
}


static void MAC_On(e_nsErr_t *p_err)
{
    MAC_Netstk->phy->on(p_err);
}


static void MAC_Off(e_nsErr_t *p_err)
{
    MAC_Netstk->phy->off(p_err);
}


static void MAC_Send(uint8_t *p_data, uint16_t len, e_nsErr_t *p_err)
{
    MAC_Netstk->phy->send(p_data, len, p_err);
}


static void MAC_Recv(uint8_t *p_data, uint16_t len, e_nsErr_t *p_err)
{
    packetbuf_clear();
    packetbuf_set_datalen(len);
    memcpy(packetbuf_dataptr(),
           p_data,
           len);

    MAC_Netstk->dllc->recv(p_data, len, p_err);
}


static void MAC_IOCtrl(e_nsIocCmd_t cmd, void *p_val, e_nsErr_t *p_err)
{
#if NETSTK_CFG_ARG_CHK_EN
    if (p_err == NULL) {
        return;
    }
#endif


    *p_err = NETSTK_ERR_NONE;
    switch (cmd) {
        case NETSTK_CMD_MAC_RSVD:
            break;

        default:
            MAC_Netstk->phy->ioctrl(cmd, p_val, p_err);
            break;
    }
}



/*
********************************************************************************
*                               END OF FILE
********************************************************************************
*/
