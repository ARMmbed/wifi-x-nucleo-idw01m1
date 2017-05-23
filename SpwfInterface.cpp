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
  * @file    SpwfInterface.cpp 
  * @author  STMicroelectronics
  * @brief   Implementation of the NetworkStack for the SPWF Device
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

#include "SpwfInterface.h"
#include "mbed_debug.h"

/** spwf_socket class
 *  Implementation of SPWF socket structure
 */
struct spwf_socket {
    int internal_id;
    int spwf_id;
    nsapi_protocol_t proto;
    bool connected;
    SocketAddress addr;
};

/**
 * @brief  SpwfSAInterface constructor
 * @param  tx: Pin USART TX
 *         rx: Pin USART RX
 *         rst: reset pin for Spwf module
 *         wkup: reset pin for Spwf module
 *         rts: Pin USART RTS
 *         debug : not used
 * @retval none
 */
SpwfSAInterface::SpwfSAInterface(PinName tx, PinName rx, bool debug)
: _spwf(tx, rx, debug),
  dbg_on(debug)
{
    memset(_ids, 0, sizeof(_ids));
    memset(_cbs, 0, sizeof(_cbs));

    _spwf.attach(this, &SpwfSAInterface::event);

    isInitialized = false;
}

/**
 * @brief  SpwfSAInterface destructor
 * @param  none
 * @retval none
 */
SpwfSAInterface::~SpwfSAInterface()
{
}

/**
 * @brief  init function
 *         initializes SPWF FW and module
 * @param  none
 * @retval error value
 */
int SpwfSAInterface::init(void) 
{
    _spwf.setTimeout(SPWF_CONNECT_TIMEOUT);

    if(_spwf.startup(0)) {
        return true;
    }
    else return NSAPI_ERROR_DEVICE_ERROR;
}

/**
 * @brief  network connect
 *        connects to Access Point
 * @param  ap: Access Point (AP) Name String
 *         pass_phrase: Password String for AP
 *         security: type of NSAPI security supported
 * @retval NSAPI Error Type
 */
int SpwfSAInterface::connect(const char *ap, 
                             const char *pass_phrase, 
                             nsapi_security_t security,
                             uint8_t channel)
{
    int mode;

    //initialize the device before connecting
    if(!isInitialized)
    {
        if(!init()) return NSAPI_ERROR_DEVICE_ERROR;
        isInitialized=true;
    }

    _spwf.setTimeout(SPWF_CONNECT_TIMEOUT);

    switch(security)
    {
        case NSAPI_SECURITY_NONE:
            mode = 0;
            pass_phrase = NULL;
            break;
        case NSAPI_SECURITY_WEP:
            mode = 1;
            break;
        case NSAPI_SECURITY_WPA:
        case NSAPI_SECURITY_WPA2:
            mode = 2;
            break;
        default:
            mode = 2;
            break;
    }

    if (!_spwf.connect(ap, pass_phrase, mode)) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    return NSAPI_ERROR_OK;
}

/**
 * @brief  network disconnect
 *         disconnects from Access Point
 * @param  none
 * @retval NSAPI Error Type
 */
int SpwfSAInterface::disconnect()
{
    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    if (!_spwf.disconnect()) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return NSAPI_ERROR_OK;
}

/** 
 * @brief  Get the local IP address
 * @param  none
 * @retval Null-terminated representation of the local IP address
 *         or null if not yet connected
 */
const char *SpwfSAInterface::get_ip_address()
{
    return _spwf.getIPAddress();
}

/** 
 * @brief  Get the MAC address
 * @param  none
 * @retval Null-terminated representation of the MAC address
 *         or null if not yet connected
 */
const char *SpwfSAInterface::get_mac_address()
{
    return _spwf.getMACAddress();
}

/**
 * @brief  open a socket handle
 * @param  handle: Pointer to handle
 *         proto: TCP/UDP protocol
 * @retval NSAPI Error Type
 */
int SpwfSAInterface::socket_open(void **handle, nsapi_protocol_t proto)
{
    int id = SPWFSA_SOCKET_COUNT;

    for (int i = 0; i < SPWFSA_SOCKET_COUNT; i++) {
        if (!_ids[i]) {
            id = i;
            _ids[i] = true;
            break;
        }
    }

    if(id == SPWFSA_SOCKET_COUNT) {
        debug_if(dbg_on, "NO Socket ID Error\r\n");
        return NSAPI_ERROR_NO_SOCKET;
    }

    struct spwf_socket *socket = new struct spwf_socket;
    if (!socket) {
        debug_if(dbg_on, "NO Socket Error\r\n");
        _ids[id] = false;
        return NSAPI_ERROR_NO_SOCKET;
    }

    socket->internal_id = id;
    socket->spwf_id = SPWFSA_SOCKET_COUNT;
    socket->proto = proto;
    socket->connected = false;

    *handle = socket;
    return NSAPI_ERROR_OK;
}

/**
 * @brief  connect to a remote socket
 * @param  handle: Pointer to socket handle
 *         addr: Address to connect to
 * @retval NSAPI Error Type
 */
int SpwfSAInterface::socket_connect(void *handle, const SocketAddress &addr)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    const char *proto = (socket->proto == NSAPI_UDP) ? "u" : "t"; //"s" for secure socket?

    if (!_spwf.open(proto, &socket->spwf_id, addr.get_ip_address(), addr.get_port())) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    socket->connected = true;
    return NSAPI_ERROR_OK;
}

int SpwfSAInterface::socket_bind(void *handle, const SocketAddress &address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::socket_listen(void *handle, int backlog)
{      
    return NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{    
    return NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::socket_close(void *handle)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    int err = NSAPI_ERROR_OK;

    MBED_ASSERT(socket->internal_id != SPWFSA_SOCKET_COUNT);

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    if(socket->spwf_id != SPWFSA_SOCKET_COUNT) {
        if (!_spwf.close(socket->spwf_id)) {
            err = NSAPI_ERROR_DEVICE_ERROR;
        }
    }

    _ids[socket->internal_id] = false;
    delete socket;

    return err;
}

/**
 * @brief  write to a socket
 * @param  handle: Pointer to handle
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes sent
 */
int SpwfSAInterface::socket_send(void *handle, const void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    _spwf.setTimeout(SPWF_SEND_TIMEOUT);

    if (!_spwf.send(socket->spwf_id, data, size)) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return size;
}

/**
 * @brief  receive data on a socket
 * @param  handle: Pointer to handle
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes read
 */
int SpwfSAInterface::socket_recv(void *handle, void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;

    _spwf.setTimeout(SPWF_RECV_TIMEOUT);

    int32_t recv = _spwf.recv(socket->spwf_id, (char*)data, (uint32_t)size);
    if (recv < 0) {
        return NSAPI_ERROR_WOULD_BLOCK;
    }

    return recv;
}

/**
 * @brief  send data to a udp socket
 * @param  handle: Pointer to handle
 *         addr: address of udp socket
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes sent
 */
int SpwfSAInterface::socket_sendto(void *handle, const SocketAddress &addr, const void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;

    if (socket->connected && socket->addr != addr) {
        _spwf.setTimeout(SPWF_MISC_TIMEOUT);
        if (!_spwf.close(socket->spwf_id)) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
        socket->connected = false;
    }

    if (!socket->connected) {
        int err = socket_connect(socket, addr);
        if (err < 0) {
            return err;
        }
        socket->addr = addr;
    }

    return socket_send(socket, data, size);
}

/**
 * @brief  receive data on a udp socket
 * @param  handle: Pointer to handle
 *         addr: address of udp socket
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes read
 */
int SpwfSAInterface::socket_recvfrom(void *handle, SocketAddress *addr, void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    int ret = socket_recv(socket, data, size);
    if (ret >= 0 && addr) {
        *addr = socket->addr;
    }

    return ret;
}

/**
 * @brief  attach function/callback to the socket
 * @param  handle: Pointer to handle
 *         callback: callback function pointer
 *         data: pointer to data
 * @retval none
 */
void SpwfSAInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    MBED_ASSERT(socket->internal_id != SPWFSA_SOCKET_COUNT);
    MBED_ASSERT(_ids[socket->internal_id]);

    _cbs[socket->internal_id].callback = callback;
    _cbs[socket->internal_id].data = data;
}

void SpwfSAInterface::event(void) {
    for (int i = 0; i < SPWFSA_SOCKET_COUNT; i++) {
        if (_cbs[i].callback) {
            _cbs[i].callback(_cbs[i].data);
        }
    }
}

int SpwfSAInterface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security) //not supported
{
    return NSAPI_ERROR_UNSUPPORTED;
}


int SpwfSAInterface::set_channel(uint8_t channel)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

int8_t SpwfSAInterface::get_rssi()
{
    return (int8_t)NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::connect()
{
    return NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::scan(WiFiAccessPoint *res, unsigned count)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
