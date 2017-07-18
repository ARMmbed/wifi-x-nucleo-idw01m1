/* SPWFInterface Example
 * Copyright (c) 2015 ARM Limited
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
 
#ifndef SPWFSA01_H
#define SPWFSA01_H

#include "ATParser.h"

class SpwfSAInterface;

/** SPWFSA01Interface class.
    This is an interface to a SPWFSA01 module.
 */
class SPWFSA01
{
public:
    SPWFSA01(PinName tx, PinName rx, SpwfSAInterface &ifce, bool debug=false);

    /**
     * Init the SPWFSA01
     *
     * @param mode mode in which to startup
     * @return true only if SPWFSA01 has started up correctly
     */
    bool startup(int mode);

    /**
     * Reset SPWFSA01
     *
     * @return true only if SPWFSA01 resets successfully
     */
    bool hw_reset(void);
    bool reset(void);

    /**
     * Enable/Disable DHCP
     *
     * @param mode mode of DHCP 2-softAP, 1-on, 0-off
     * @return true only if SPWFSA01 enables/disables DHCP successfully
     */
    bool dhcp(int mode);

    /**
     * Connect SPWFSA01 to AP
     *
     * @param ap the name of the AP
     * @param passPhrase the password of AP
     * @param securityMode the security mode of AP (WPA/WPA2, WEP, Open)
     * @return true only if SPWFSA01 is connected successfully
     */
    bool connect(const char *ap, const char *passPhrase, int securityMode);

    /**
     * Disconnect SPWFSA01 from AP
     *
     * @return true only if SPWFSA01 is disconnected successfully
     */
    bool disconnect(void);

    /**
     * Get the IP address of SPWFSA01
     *
     * @return null-teriminated IP address or null if no IP address is assigned
     */
    const char *getIPAddress(void);

    /**
     * Get the MAC address of SPWFSA01
     *
     * @return null-terminated MAC address or null if no MAC address is assigned
     */
    const char *getMACAddress(void);

    /** Get the local gateway
     *
     *  @return         Null-terminated representation of the local gateway
     *                  or null if no network mask has been recieved
     */
    const char *getGateway(void);

    /** Get the local network mask
     *
     *  @return         Null-terminated representation of the local network mask
     *                  or null if no network mask has been recieved
     */
    const char *getNetmask(void);

    /**
     * Check if SPWFSA01 is connected
     *
     * @return true only if the chip has an IP address
     */
    bool isConnected(void);

    /** Scan for available networks
     *
     * @param  ap    Pointer to allocated array to store discovered AP
     * @param  limit Size of allocated @a res array, or 0 to only count available AP
     * @return       Number of entries in @a res, or if @a count was 0 number of available networks, negative on error
     *               see @a nsapi_error
     */
    nsapi_size_or_error_t scan(WiFiAccessPoint *res, unsigned limit);

    /**
     * Open a socketed connection
     *
     * @param type the type of socket to open "u" (UDP) or "t" (TCP)
     * @param id id to get the new socket number, valid 0-7
     * @param port port to open connection with
     * @param addr the IP address of the destination
     * @return true only if socket opened successfully
     */
    bool open(const char *type, int* id, const char* addr, int port);

    /**
     * Sends data to an open socket
     *
     * @param id id of socket to send to
     * @param data data to be sent
     * @param amount amount of data to be sent - max 1024
     * @return true only if data sent successfully
     */
    bool send(int id, const void *data, uint32_t amount);

    /**
     * Receives data from an open socket
     *
     * @param id id to receive from
     * @param data placeholder for returned information
     * @param amount number of bytes to be received
     * @return the number of bytes received
     */
    int32_t recv(int id, void *data, uint32_t amount);

    /**
     * Closes a socket
     *
     * @param id id of socket to close, valid only 0-4
     * @return true only if socket is closed successfully
     */
    bool close(int id);

    /**
     * Allows timeout to be changed between commands
     *
     * @param timeout_ms timeout of the connection
     */
    void setTimeout(uint32_t timeout_ms);

    /**
     * Checks if data is available
     */
    bool readable(void);

    /**
     * Checks if data can be written
     */
    bool writeable(void);

    /**
     * Attach a function to call whenever network state has changed
     *
     * @param func A pointer to a void function, or 0 to set as none
     */
    void attach(Callback<void()> func);

    /**
     * Attach a function to call whenever network state has changed
     *
     * @param obj pointer to the object to call the member function on
     * @param method pointer to the member function to call
     */
    template <typename T, typename M>
    void attach(T *obj, M method) {
        attach(Callback<void()>(obj, method));
    }

private:
    BufferedSerial _serial;
    ATParser _parser;
    DigitalOut _wakeup;
    DigitalOut _reset;
    int _timeout;
    bool _dbg_on;
    bool _call_event_callback_blocked;
    int _pending_sockets_bitmap;
    bool _network_lost_flag;
    SpwfSAInterface &_associated_interface;
    Callback<void()> _callback_func;

    struct packet {
        struct packet *next;
        int id;
        uint32_t len;
        // data follows
    } *_packets, **_packets_end;

    void _packet_handler_th(void);
    void _execute_bottom_halves(void);
    void _pending_data_handler(void);
    void _network_lost_handler_th(void);
    void _network_lost_handler_bh(void);
    void _hard_fault_handler(void);
    void _wifi_hwfault_handler(void);
    void _event_handler(void);
    void _sock_closed_handler(void);
    void _wait_console_active(void);
    int _read_in(char*, int, uint32_t);
    int _read_len(int);
    int _flush_in(char*, int);
    bool _winds_off(void);
    void _winds_on(void);
    void _read_in_pending(void);
    int _read_in_packet(int spwf_id);
    bool _read_in_packet(int spwf_id, int amount);
    void _read_in_pending_winds(void);
    void _recover_from_hard_faults(void);
    void _free_packets(int spwf_id);
    void _free_all_packets(void);
    bool _recv_ap(nsapi_wifi_ap_t *ap);

    bool _recv_delim_lf(void) {
        return (_parser.getc() == '\x0a');
    }

    bool _recv_delim_cr(void) {
        return (_parser.getc() == '\x0d');
    }

    bool _recv_delim_cr_lf(void) {
        return _recv_delim_cr() && _recv_delim_lf();
    }

    bool _recv_ok(void) {
        return _parser.recv("OK\x0d") && _recv_delim_lf();
    }

    bool _is_data_pending(void) {
        if(_pending_sockets_bitmap != 0) return true;
        else return false;
    }

    void _set_pending_data(int spwf_id) {
        _pending_sockets_bitmap |= (1 << spwf_id);
    }

    void _clear_pending_data(int spwf_id) {
        _pending_sockets_bitmap &= ~(1 << spwf_id);
    }

    bool _is_data_pending(int spwf_id) {
        return (_pending_sockets_bitmap & (1 << spwf_id)) ? true : false;
    }

    bool _is_event_callback_blocked(void) {
        return _call_event_callback_blocked;
    }

    void _block_event_callback(void) {
        MBED_ASSERT(!_call_event_callback_blocked);
        _call_event_callback_blocked = true;
    }

    void _unblock_event_callback(void) {
        MBED_ASSERT(_call_event_callback_blocked);
        _call_event_callback_blocked = false;
    }

    void _packet_handler_bh(void) {
        /* read in other eventually pending packages */
        _read_in_pending();
    }

    char _ip_buffer[16];
    char _gateway_buffer[16];
    char _netmask_buffer[16];
    char _mac_buffer[18];
};

/* Helper class to execute something whenever entering/leaving a basic block */
class BlockExecuter {
public:
    BlockExecuter(Callback<void()> exit_cb, Callback<void()> enter_cb = Callback<void()>()) :
        _exit_cb(exit_cb) {
        if((bool)enter_cb) enter_cb();
    }

    ~BlockExecuter(void) {
        _exit_cb();
    }

private:
    Callback<void()> _exit_cb;
};

#define BH_HANDLER \
        BlockExecuter bh_handler(Callback<void()>(this, &SPWFSA01::_execute_bottom_halves))

#endif  //SPWFSA01_H
