/*
 * emb6 is licensed under the 3-clause BSD license. This license gives everyone
 * the right to use and distribute the code, either in binary or source code
 * format, as long as the copyright license is retained in the source code.
 *
 * The emb6 is derived from the Contiki OS platform with the explicit approval
 * from Adam Dunkels. However, emb6 is made independent from the OS through the
 * removal of protothreads. In addition, APIs are made more flexible to gain
 * more adaptivity during run-time.
 *
 * The license text is:
 *
 * Copyright (c) 2015,
 * Hochschule Offenburg, University of Applied Sciences
 * Laboratory Embedded Systems and Communications Electronics.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**
 * \addtogroup native_radio
 * @{
 */
/*============================================================================*/
/*! \file   native.c

 \author Artem Yushev 

 \brief  Fake radio transceiver based on LCM IPC.

 \version 1.1
 */
/*============================================================================*/

/*==============================================================================
 INCLUDE FILES
 ==============================================================================*/
#include "emb6.h"
#include "emb6_conf.h"
#include "bsp.h"
#include "packetbuf.h"
#include "tcpip.h"
#include "etimer.h"
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
/* Ligthweight Communication and Marshalling (LCM) is a library to perform
 communication between processes  */
#include <lcm/lcm.h>
/*==============================================================================
 MACROS
 ==============================================================================*/
#define     LOGGER_ENABLE                 LOGGER_RADIO
#include    "logger.h"
#define     __ADDRLEN__                   2
#define     NODE_INFO_MAX                 2048

/*==============================================================================
 ENUMS
 ==============================================================================*/

/*==============================================================================
 VARIABLE DECLARATIONS
 ==============================================================================*/
static struct etimer ps_nativeTmr;
/* Pointer to the lmac structure */
static s_nsLowMac_t* p_lmac = NULL;
extern uip_lladdr_t uip_lladdr;
static lcm_t *ps_lcm;
static char pc_publish_ch[NODE_INFO_MAX];
static char *pc_subscribe_ch;
static lcm_subscription_t *subscr;
/*==============================================================================
 GLOBAL CONSTANTS
 ==============================================================================*/
/*==============================================================================
 LOCAL FUNCTION PROTOTYPES
 ==============================================================================*/
/* Radio transceiver local functions */
static void _printAndExit( const char* rpc_reason );
static int8_t _native_on( void );
static int8_t _native_off( void );
static int8_t _native_init( s_ns_t* p_netStack );
static int8_t _native_send( const void *pr_payload, uint8_t c_len );
static void _native_read( const lcm_recv_buf_t *rbuf, const char * channel,
        void * p_macAddr );
static void _native_handler( c_event_t c_event, p_data_t p_data );
static void _beautiful_split_messages( const lcm_recv_buf_t *rps_rbuf,
        const char * rpc_channel, void * userdata );
static void _beautiful_comand_parser( const char *line);
/*==============================================================================
 STRUCTURES AND OTHER TYPEDEFS
 ==============================================================================*/
const s_nsIf_t native_driver = {
        "native_driver",
        _native_init,
        _native_send,
        _native_on,
        _native_off, };
/*==============================================================================
 LOCAL FUNCTIONS
 ==============================================================================*/
/*----------------------------------------------------------------------------*/
/** \brief  This function reports the error and exits back to the shell.
 *
 *  \param  rpc_reason  Error to show
 *  \return Node
 */
/*----------------------------------------------------------------------------*/
static void _printAndExit( const char* rpc_reason )
{
    // TODO: LOG_ERR(...);
    fputs( strerror( errno ), stderr );
    fputs( ": ", stderr );
    fputs( rpc_reason, stderr );
    fputc( '\n', stderr );
    exit( 1 );
}

/*----------------------------------------------------------------------------*/
/** \brief  NATIVE transport initialization for IPC
 *
 *  \param  p_netStack    Pointer to s network stack.
 *  \return int8_t        Status code.
 */
/*----------------------------------------------------------------------------*/
static int8_t _native_init( s_ns_t* p_netStack )
{
    linkaddr_t un_addr;
    uint8_t l_error;
    FILE* fp;
    char pc_node_info[NODE_INFO_MAX];
    char* pch;

    uint16_t addr;    // mac address of node read from configuration file
    uint8_t  addr_6;  // high byte of two last parts of mac address
    uint8_t  addr_7;  // low byte of two last parts of mac address

    LOG1_INFO( "Try to initialize Broadcasting Client for native radio driver" );

    /* Please refer to lcm_create() reference. Default parameter is taken
     from there */
    ps_lcm = lcm_create( NULL );

    if( !ps_lcm )
        _printAndExit("LCM init failed");

    /* reset channel name */
    memset( pc_publish_ch, 0, NODE_INFO_MAX );

    /* Read configuration file */
    fp = fopen( "lcmnetwork.conf", "r" );

    if( fp == NULL )
    {
        _printAndExit( "Can't open this file\n" );
    }

    /* assemble the parser command */
    while( !feof(fp) )
    {
        memset(pc_node_info, 0, NODE_INFO_MAX);
        fgets( pc_node_info, NODE_INFO_MAX, fp );
        if( pc_node_info[0] == '#' ) continue;

        pch = strtok (pc_node_info," \t\n,");
        if( pch == NULL ) continue;

        /* get address */
        sscanf( pch, "%hx", &addr );

        /* split mac address read from the file into two parts */
        addr_7 = (uint8_t)addr;
        addr_6 = (uint8_t)(addr >> 8);

        if( addr_6 != mac_phy_config.mac_address[6] ||
            addr_7 != mac_phy_config.mac_address[7] )
            continue;

        /* read for the subscribe channel */
        if ( pch != NULL )
        {
            int tmpChLen = strlen(pch) + 10;
            char* tmpCh = malloc( tmpChLen );
            pc_subscribe_ch = malloc( tmpChLen );
            if( tmpCh != NULL )
            {
                snprintf( tmpCh, tmpChLen, ".*_%s_.*", pch );
                subscr = lcm_subscribe( ps_lcm, tmpCh, _beautiful_split_messages, NULL );
                LOG1_INFO("Subscription channel =  %s", tmpCh);
                strcpy(pc_subscribe_ch, tmpCh);
                free( tmpCh );
            }
            else
            {
                _printAndExit( "Can't create virtual channel.\n" );
            }

            pch = strtok ( NULL, " \t\n," );
        }

        /* read for the public channel */
        while ( pch != NULL )
        {
            if( pch != NULL )
            snprintf( pc_publish_ch + strlen(pc_publish_ch),
                    (NODE_INFO_MAX-strlen(pc_publish_ch)), "_%s_", pch );
            pch = strtok ( NULL, " \t\n," );
        }
        LOG1_INFO("Publication channel = %s", pc_publish_ch);
    }

    /* Close the file */
    fclose(fp);

    LOG1_OK( "Native driver init" );

    /* Mac address should not be NULL pointer, although it can't be, but still
     *  */
    if( mac_phy_config.mac_address == NULL )
    {
        _printAndExit( "MAC address is NULL" );
    }

    /* Initialise global lladdr structure with a given mac */
    memcpy( (void *)&un_addr.u8, &mac_phy_config.mac_address, 8 );
    memcpy( &uip_lladdr.addr, &un_addr.u8, 8 );
    linkaddr_set_node_addr( &un_addr );

    LOG1_INFO( "MAC address %x:%x:%x:%x:%x:%x:%x:%x", un_addr.u8[0],
            un_addr.u8[1], un_addr.u8[2], un_addr.u8[3], un_addr.u8[4],
            un_addr.u8[5], un_addr.u8[6], un_addr.u8[7] );

    if( p_netStack->lmac != NULL )
    {
        p_lmac = p_netStack->lmac;
        l_error = 1;
    }
    else
    {
        _printAndExit( "Bad lmac pointer" );
    }

    /* Start the packet receive process */
    etimer_set( &ps_nativeTmr, 10, _native_handler );

    return l_error;
} /* _native_init() */

/*----------------------------------------------------------------------------*/
/** \brief  NATIVE transport message send
 *          Idea behind is that we concatenate to payload last two bytes of a
 *          MAC address in order to filter our own packets on a receiving side
 *  \param  pr_payload    Pointer to a payload.
 *  \param  c_len         Length of a payload
 *  \return int8_t        Status code.
 */
/*----------------------------------------------------------------------------*/
static int8_t _native_send( const void *pr_payload, uint8_t c_len )
{
    uint32_t status;

    status = lcm_publish( ps_lcm, pc_publish_ch, pr_payload, c_len );

    /* Return execution status to a caller */
    if( status == -1 )
    {
        LOG_ERR( "Send packet failed" );
        return RADIO_TX_ERR;
    }
    else
    {
        LOG_OK( "TX packet [%d]", c_len );
        LOG2_HEXDUMP( pr_payload, c_len );
        return RADIO_TX_OK;
    }
} /* _native_send() */

/*----------------------------------------------------------------------------*/
/** \brief  NATIVE transport message reception
 *          Idea behind is that we concatenate to payload last two bytes of a
 *          MAC address in order to filter our own packets on a receiving side
 *  \param  rps_rbuf      Pointer to a payload.
 *  \param  rpc_channel   Reception channel
 *  \param  userdata      Not used.
 *  \return void
 */
/*----------------------------------------------------------------------------*/
static void _native_read( const lcm_recv_buf_t *rps_rbuf,
        const char * rpc_channel, void * userdata )
{
    uint16_t i_dSize = rps_rbuf->data_size;

    /* Clear buffer where to store received payload */
    packetbuf_clear();

    /* Check whether recieved packet is not too long */
    if( i_dSize > PACKETBUF_SIZE )
    {
        LOG_ERR( "Received packet too long" );
    }
    else
    {
        memcpy( packetbuf_dataptr(), rps_rbuf->data, i_dSize );
        LOG_OK( "RX packet [%d]", i_dSize);
        LOG2_HEXDUMP( packetbuf_dataptr(), i_dSize  );
        if( ( rps_rbuf->data_size > 0 ) && ( p_lmac != NULL ) )
        {
            packetbuf_set_datalen( i_dSize );
            p_lmac->input();
        }
        else
        {
            LOG_ERR( "Failed to receive packet" );
        }
    }
} /* _native_read() */

/*----------------------------------------------------------------------------*/
/** \brief  NATIVE transport wrapper function
 *  \return 0
 */
/*----------------------------------------------------------------------------*/
static int8_t _native_on( void )
{
    return 0;
} /* _native_on() */

/*----------------------------------------------------------------------------*/
/** \brief  NATIVE transport wrapper function.
 *          lcm structure destroy can be invoked here
 *  \return 0
 */
/*----------------------------------------------------------------------------*/
static int8_t _native_off( void )
{
    return 0;
} /* _native_off() */

/*----------------------------------------------------------------------------*/
/** \brief  NATIVE transport handler for periodic polling
 *          triggered every 10 msec
 *  \param  c_event       Source of an event.
 *  \param  p_data        Pointer to a data
 *  \return void
 */
/*----------------------------------------------------------------------------*/
static void _native_handler( c_event_t c_event, p_data_t p_data )
{
    int32_t lcm_fd;
    struct timeval s_tv;
    s_tv.tv_sec = 0;
    s_tv.tv_usec = 10;
    fd_set fds;

    if( etimer_expired( &ps_nativeTmr ) )
    {
        /* We can't use lcm_handle trigger every time, as
         * it's a blocking operation. We should instead check whether a lcm file
         * descriptor is available for reading. We put 10 usec as a timeout.
         */
        lcm_fd = lcm_get_fileno( ps_lcm );
        FD_ZERO( &fds );
        FD_SET( lcm_fd, &fds );

        select( lcm_fd + 1, &fds, 0, 0, &s_tv );
        /* If descriptor is available for reading then there is a incoming data
         * to read.
         */
        if( FD_ISSET( lcm_fd, &fds ) )
        {
            lcm_handle( ps_lcm );
        }
        /* Restart a timer anyway. */
        etimer_restart( &ps_nativeTmr );

    }
}

/*----------------------------------------------------------------------------*/
/** \brief  "BEAUTIFUL" split messages reception
 *          You think this is funny?
 *  \param  rps_rbuf      Pointer to a payload.
 *  \param  rpc_channel   Reception channel
 *  \param  userdata      Not used.
 *  \return void
 */
/*----------------------------------------------------------------------------*/
static void _beautiful_split_messages( const lcm_recv_buf_t *rps_rbuf,
        const char * rpc_channel, void * userdata )
{
    const char *message = (const char *)rps_rbuf->data;
    if (strncmp(message, "CMD line", 4) == 0)
    {
        int length = strlen(message)-3;
        char *line = (char *)malloc(length*sizeof(char));
        memset(line, '\0', length);
        strcpy(line, message+4);
        //LOG_INFO("%s:\n\t\"%s\"\n", "I've got a CMD", line);
        printf("%s:\n\t\"%s\"\n", "I've got a CMD", line);
        _beautiful_comand_parser(line);
    }
    else
    {
        _native_read(rps_rbuf, rpc_channel, userdata);
    }
    return;
} /* _beautiful_split_messages() */

/*----------------------------------------------------------------------------*/
/** \brief  "BEAUTIFUL" split messages reception
 *          You think this is funny?
 *  \param  rps_rbuf      Pointer to a payload.
 *  \param  rpc_channel   Reception channel
 *  \param  userdata      Not used.
 *  \return void
 */
/*----------------------------------------------------------------------------*/
static void _beautiful_comand_parser( const char *line)
{
    if (strncmp(line, "subscribe name", 10) == 0)
    {
        int length = strlen(line)-9;
        char *new_channel = (char *)malloc(length*sizeof(char));
        memset(new_channel, '\0', length);
        strcpy(new_channel, line+10);
        if (strcmp(new_channel, pc_subscribe_ch) == 0) return;
        pc_subscribe_ch = new_channel;
        lcm_unsubscribe(ps_lcm, subscr);
        subscr = lcm_subscribe( ps_lcm, pc_subscribe_ch, _beautiful_split_messages, NULL );
    }
    if (strncmp(line, "publish name", 8) == 0)
    {
        int length = strlen(line)-7;
        char *new_channel = (char *)malloc(length*sizeof(char));
        memset(new_channel, '\0', length);
        strcpy(new_channel, line+8);
        if (strcmp(new_channel, pc_publish_ch) == 0) return;
        memset( pc_publish_ch, '\0', NODE_INFO_MAX );
        strcpy(pc_publish_ch, new_channel);
    }
    if (strncmp(line, "exit", 4) == 0)
    {
        _printAndExit("I've got command to EXIT. Bye!\n");
    }
    return;
} /* _beautiful_split_messages() */

/*==============================================================================
 API FUNCTIONS
 ==============================================================================*/
/** @} */
