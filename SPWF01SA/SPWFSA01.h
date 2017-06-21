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

    /**
     * Check if SPWFSA01 is connected
     *
     * @return true only if the chip has an IP address
     */
    bool isConnected(void);

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
    bool readable();

    /**
     * Checks if data can be written
     */
    bool writeable();

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
    rtos::Semaphore _rx_sem;
    bool _release_rx_sem;
    int _disassoc_handler_recursive_cnt;
    int _timeout;
    bool _dbg_on;
    bool _send_at;
    bool _read_in_pending_blocked;
    int _call_event_callback_blocked_cnt;
    int _pending_sockets_bitmap;
    SpwfSAInterface &_associated_interface;
    Callback<void()> _callback_func;

    struct packet {
        struct packet *next;
        int id;
        uint32_t len;
        // data follows
    } *_packets, **_packets_end;

    void _packet_handler();
    void _pending_data_handler();
    void _disassociation_handler();
    void _hard_fault_handler();
    void _event_handler();
    void _sock_closed_handler();
    void _wait_console_active();
    int _read_in(char*, int, uint32_t);
    int _read_len(int);
    int _flush_in(char*, int);
    int _block_async_indications(void);
    void _read_in_pending(void);
    int _read_in_packet(int spwf_id);
    bool _read_in_packet(int spwf_id, int amount);
    void _free_packets(int spwf_id);

    bool _recv_delim() {
        return (_parser.getc() == '\n');
    }

    bool _recv_ok() {
        return _parser.recv("OK\r") && _recv_delim();
    }

    void _block_read_in_pending() {
        MBED_ASSERT(!_read_in_pending_blocked);
        _read_in_pending_blocked = true;
    }

    void _unblock_read_in_pending() {
        MBED_ASSERT(_read_in_pending_blocked);
        _read_in_pending_blocked = false;
    }

    bool _is_read_in_pending_blocked() {
        return _read_in_pending_blocked;
    }

    bool _is_data_pending() {
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

    bool _is_event_callback_blocked() {
        return (_call_event_callback_blocked_cnt != 0);
    }

    void _block_event_callback() {
        _call_event_callback_blocked_cnt++;
    }

    void _unblock_event_callback() {
        MBED_ASSERT(_call_event_callback_blocked_cnt > 0);
        _call_event_callback_blocked_cnt--;
    }

    char _ip_buffer[16];
    char _mac_buffer[18];
};

/* Helper class to execute something whenever entering/leaving a basic block */
class BlockExecuter {
public:
    BlockExecuter(Callback<void()> exit_cb, Callback<void()> enter_cb = Callback<void()>()) :
    _exit_cb(exit_cb) {
        if((bool)enter_cb) enter_cb();
    }

    ~BlockExecuter() {
        _exit_cb();
    }

private:
    Callback<void()> _exit_cb;
};

#endif  //SPWFSA01_H
