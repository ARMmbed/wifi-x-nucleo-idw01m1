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
    : _spwf(tx, rx, *this, debug),
      _dbg_on(debug)
{
    inner_constructor();
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
nsapi_error_t SpwfSAInterface::init(void)
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
nsapi_error_t SpwfSAInterface::connect(const char *ap,
                                       const char *pass_phrase,
                                       nsapi_security_t security,
                                       uint8_t channel)
{
    int mode;

    //initialize the device before connecting
    if(!_isInitialized)
    {
        if(!init()) return NSAPI_ERROR_DEVICE_ERROR;
        _isInitialized=true;
    }

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

    // First: disconnect
    if(_connected_to_network) {
        if(!disconnect()) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
    }

    // Then: (re-)connect
    _spwf.setTimeout(SPWF_CONNECT_TIMEOUT);

    if (!_spwf.connect(ap, pass_phrase, mode)) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    _connected_to_network = true;
    return NSAPI_ERROR_OK;
}

/**
 * @brief  network disconnect
 *         disconnects from Access Point
 * @param  none
 * @retval NSAPI Error Type
 */
nsapi_error_t SpwfSAInterface::disconnect()
{
    CHECK_NOT_CONNECTED_ERR();

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    if (!_spwf.disconnect()) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    _connected_to_network = false;
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
nsapi_error_t SpwfSAInterface::socket_open(void **handle, nsapi_protocol_t proto)
{
    int id;

    for (id = 0; id < SPWFSA_SOCKET_COUNT; id++) {
        if (_ids[id].internal_id == SPWFSA_SOCKET_COUNT) break;
    }

    if(id == SPWFSA_SOCKET_COUNT) {
        debug_if(_dbg_on, "NO Socket ID Error\r\n");
        return NSAPI_ERROR_NO_SOCKET;
    }

    spwf_socket_t *socket = &_ids[id];
    socket->internal_id = id;
    socket->spwf_id = SPWFSA_SOCKET_COUNT;
    socket->proto = proto;

    *handle = socket;
    return NSAPI_ERROR_OK;
}

/**
 * @brief  connect to a remote socket
 * @param  handle: Pointer to socket handle
 *         addr: Address to connect to
 * @retval NSAPI Error Type
 */
nsapi_error_t SpwfSAInterface::socket_connect(void *handle, const SocketAddress &addr)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    if(socket->spwf_id != SPWFSA_SOCKET_COUNT) return NSAPI_ERROR_IS_CONNECTED;

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    const char *proto = (socket->proto == NSAPI_UDP) ? "u" : "t"; //"s" for secure socket?

    if (!_spwf.open(proto, &socket->spwf_id, addr.get_ip_address(), addr.get_port())) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t SpwfSAInterface::socket_bind(void *handle, const SocketAddress &address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::socket_listen(void *handle, int backlog)
{      
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{    
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::socket_close(void *handle)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;
    nsapi_error_t err = NSAPI_ERROR_OK;

    if(socket->internal_id == SPWFSA_SOCKET_COUNT) return NSAPI_ERROR_NO_SOCKET;

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    if(socket->spwf_id != SPWFSA_SOCKET_COUNT) {
        if (!_spwf.close(socket->spwf_id)) {
            err = NSAPI_ERROR_DEVICE_ERROR;
        }
    }

    _ids[socket->internal_id].internal_id = SPWFSA_SOCKET_COUNT;

    return err;
}

/**
 * @brief  write to a socket
 * @param  handle: Pointer to handle
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes sent
 */
nsapi_error_t SpwfSAInterface::socket_send(void *handle, const void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    CHECK_NOT_CONNECTED_ERR();

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
nsapi_error_t SpwfSAInterface::socket_recv(void *handle, void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    CHECK_NOT_CONNECTED_ERR();

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
nsapi_error_t SpwfSAInterface::socket_sendto(void *handle, const SocketAddress &addr, const void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    CHECK_NOT_CONNECTED_ERR();

    if ((socket->spwf_id != SPWFSA_SOCKET_COUNT) && (socket->addr != addr)) {
        _spwf.setTimeout(SPWF_MISC_TIMEOUT);
        if (!_spwf.close(socket->spwf_id)) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
        socket->spwf_id = SPWFSA_SOCKET_COUNT;
    }

    if (socket->spwf_id == SPWFSA_SOCKET_COUNT) {
        nsapi_error_t err = socket_connect(socket, addr);
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
nsapi_error_t SpwfSAInterface::socket_recvfrom(void *handle, SocketAddress *addr, void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;
    nsapi_error_t ret;

    CHECK_NOT_CONNECTED_ERR();

    ret = socket_recv(socket, data, size);
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
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    if(socket->internal_id == SPWFSA_SOCKET_COUNT) return; // might happen after module hard fault

    _cbs[socket->internal_id].callback = callback;
    _cbs[socket->internal_id].data = data;
}

void SpwfSAInterface::event(void) {
    for (int i = 0; i < SPWFSA_SOCKET_COUNT; i++) {
        if (_cbs[i].callback && (_ids[i].internal_id != SPWFSA_SOCKET_COUNT)) {
            _cbs[i].callback(_cbs[i].data);
        }
    }
}

nsapi_error_t SpwfSAInterface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security) //not supported
{
    return NSAPI_ERROR_UNSUPPORTED;
}


nsapi_error_t SpwfSAInterface::set_channel(uint8_t channel)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

int8_t SpwfSAInterface::get_rssi()
{
    return 0;  // betzw - TODO: not yet supported!
}

nsapi_error_t SpwfSAInterface::connect()
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::scan(WiFiAccessPoint *res, unsigned count)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
