/* mbed Microcontroller Library
* Copyright (c) 20015 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
  ******************************************************************************
  * @file    SpwfInterface.h 
  * @author  STMicroelectronics
  * @brief   Header file of the NetworkStack for the SPWF Device
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
  ******************************************************************************
  */
  
#ifndef SPWFSA_INTERFACE_H
#define SPWFSA_INTERFACE_H

#include "mbed.h"
#include "SPWFSA01.h"

/* Max number of sockets */
#define SPWFSA_SOCKET_COUNT 8

// Various timeouts for different SPWF operations
#define SPWF_CONNECT_TIMEOUT    60000
#define SPWF_NETLOST_TIMEOUT    30000
#define SPWF_SEND_TIMEOUT       500
#define SPWF_MISC_TIMEOUT       501
#define SPWF_RECV_TIMEOUT       100

/** SpwfSAInterface class
 *  Implementation of the NetworkStack for the SPWF Device
 *  NOTE - betzw - TODO: MUST become singleton!
 */
class SpwfSAInterface : public NetworkStack, public WiFiInterface
{
public:

    SpwfSAInterface(PinName tx, PinName rx, bool debug = false);
    virtual     ~SpwfSAInterface();

    // Implementation of WiFiInterface
    virtual     nsapi_error_t connect(const char *ssid,
                                      const char *pass,
                                      nsapi_security_t security = NSAPI_SECURITY_NONE,
                                      uint8_t channel = 0);

    virtual     nsapi_error_t disconnect();
    virtual     const char *get_mac_address();

    //Implementation of NetworkStack
    virtual     const char *get_ip_address();

protected:
    //Implementation of NetworkStack
    virtual     nsapi_error_t socket_open(void **handle, nsapi_protocol_t proto);
    virtual     nsapi_error_t socket_close(void *handle);
    virtual     nsapi_error_t socket_bind(void *handle, const SocketAddress &address);  //not supported
    virtual     nsapi_error_t socket_listen(void *handle, int backlog);
    virtual     nsapi_error_t socket_connect(void *handle, const SocketAddress &address);
    //virtual     nsapi_error_t socket_accept(nsapi_socket_t server, void **handle);
    virtual	    nsapi_error_t socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address=0);  // not supported
    virtual     nsapi_error_t socket_send(void *handle, const void *data, unsigned size);
    virtual     nsapi_error_t socket_recv(void *handle, void *data, unsigned size);
    virtual     nsapi_error_t socket_sendto(void *handle, const SocketAddress &address, const void *data, unsigned size);
    virtual     nsapi_error_t socket_recvfrom(void *handle, SocketAddress *address, void *buffer, unsigned size);
    virtual     void socket_attach(void *handle, void (*callback)(void *), void *data);
    /* WiFiInterface */
    virtual 		nsapi_error_t set_credentials(const char *ssid, const char *pass, nsapi_security_t security = NSAPI_SECURITY_NONE); //not supported
    virtual 		nsapi_error_t set_channel(uint8_t channel);
    virtual 		int8_t get_rssi();
    virtual 		nsapi_error_t connect();
    virtual 		nsapi_error_t scan(WiFiAccessPoint *res, unsigned count);


    virtual NetworkStack *get_stack()
    {
        return this;
    }

private:
    /** spwf_socket class
     *  Implementation of SPWF socket structure
     */
    typedef struct spwf_socket {
        int8_t internal_id;
        int spwf_id;
        nsapi_protocol_t proto;
        SocketAddress addr;
    } spwf_socket_t;

    SPWFSA01 _spwf;

    bool _isInitialized;
    bool _dbg_on;
    bool _connected_to_network;

    spwf_socket_t _ids[SPWFSA_SOCKET_COUNT];
    struct {
        void (*callback)(void *);
        void *data;
    } _cbs[SPWFSA_SOCKET_COUNT];
    int _internal_ids[SPWFSA_SOCKET_COUNT];

private:
    void event(void);
    nsapi_error_t init(void);

    int get_internal_id(int spwf_id) {
        return _internal_ids[spwf_id];
    }

    /* Called at initialization or after module hard fault */
    void inner_constructor() {
        memset(_ids, 0, sizeof(_ids));
        memset(_cbs, 0, sizeof(_cbs));
        memset(_internal_ids, 0, sizeof(_internal_ids));

        for (int internal_id = 0; internal_id < SPWFSA_SOCKET_COUNT; internal_id++) {
            _ids[internal_id].internal_id = SPWFSA_SOCKET_COUNT;
            _ids[internal_id].spwf_id = SPWFSA_SOCKET_COUNT;
        }

        _spwf.attach(this, &SpwfSAInterface::event);

        _connected_to_network = false;
        _isInitialized = false;
    }

private:
    friend class SPWFSA01;
};

#define CHECK_NOT_CONNECTED_ERR() { \
        if(!_connected_to_network) return NSAPI_ERROR_NO_CONNECTION; \
} \


#endif
