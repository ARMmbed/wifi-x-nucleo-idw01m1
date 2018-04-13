/* SPWFSA04 Device
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

#include "SPWFSA04.h"
#include "SpwfSAInterface.h"
#include "mbed_debug.h"

#if MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1

SPWFSA04::SPWFSA04(PinName tx, PinName rx,
                   PinName rts, PinName cts,
                   SpwfSAInterface &ifce, bool debug,
                   PinName wakeup, PinName reset)
: SPWFSAxx(tx, rx, rts, cts, ifce, debug, wakeup, reset) {
}

bool SPWFSA04::open(const char *type, int* spwf_id, const char* addr, int port)
{
    int socket_id;
    int value;
    int trials;

    if(!_parser.send("AT+S.SOCKON=%s,%d,NULL,%s", addr, port, type))
    {
        debug_if(_dbg_on, "\r\nSPWF> `SPWFSA04::open`: error opening socket (%d)\r\n", __LINE__);
        return false;
    }

    /* handle both response possibilities here before returning
     * otherwise module seems to remain in inconsistent state.
     */

    if(!_parser.recv("AT-S.")) { // get prefix
        debug_if(_dbg_on, "\r\nSPWF> `SPWFSA04::open`: error opening socket (%d)\r\n", __LINE__);
        empty_rx_buffer();
        return false;
    }

    /* wait for next character */
    trials = 0;
    while((value = _parser.getc()) < 0) {
        if(++trials >= SPWFXX_MAX_TRIALS) {
            debug("\r\nSPWF> `SPWFSA04::open`: error opening socket (%d)\r\n", __LINE__);
            empty_rx_buffer();
            return false;
        }
    }

    switch(value) {
        case 'O':
            /* get next character */
            value = _parser.getc();
            if(value != 'n') {
                debug_if(_dbg_on, "\r\nSPWF> `SPWFSA04::open`: error opening socket (%d) (%d, \'%c\')\r\n",
                         __LINE__, value, value);
                empty_rx_buffer();
                return false;
            }

            /* get socket id */
            if(!(_parser.recv(":%*u.%*u.%*u.%*u:%d\n", &socket_id)
                    && _recv_delim_lf()
                    && _recv_ok())) {
                debug_if(_dbg_on, "\r\nSPWF> `SPWFSA04::open`: error opening socket (%d)\r\n", __LINE__);
                empty_rx_buffer();
                return false;
            }
            debug_if(_dbg_on, "AT^ AT-S.On:%s:%d\r\n", addr, socket_id);

            *spwf_id = socket_id;
            return true;
        case 'E':
            int err_nr;
            if(_parser.recv("RROR:%d:%255[^\n]\n", &err_nr, _msg_buffer) && _recv_delim_lf()) {
                debug_if(_dbg_on, "AT^ AT-S.ERROR:%d:%s (%d)\r\n", err_nr, _msg_buffer, __LINE__);
            } else {
                debug_if(_dbg_on, "\r\nSPWF> `SPWFSA04::open`: error opening socket (%d)\r\n", __LINE__);
                empty_rx_buffer();
            }
            break;
        default:
            debug_if(_dbg_on, "\r\nSPWF> `SPWFSA04::open`: error opening socket (%d) (%d, \'%c\')\r\n",
                     __LINE__, value, value);
            empty_rx_buffer();
            break;
    }

    return false;
}

int SPWFSA04::_read_in(char* buffer, int spwf_id, uint32_t amount) {
    int ret = -1;
    int received, cumulative;

    MBED_ASSERT(buffer != NULL);

    /* block asynchronous indications */
    if(!_winds_off()) {
        return -1;
    }

    /* read in data */
    if(_parser.send("AT+S.SOCKR=%d,%d", spwf_id, (unsigned int)amount)) {
        if(!(_parser.recv("AT-S.Reading:%d:%d\n", &received, &cumulative) &&
                _recv_delim_lf())) {
            debug_if(_dbg_on, "\r\nSPWF> failed to receive AT-S.Reading (%s, %d)\r\n", __func__, __LINE__);
            empty_rx_buffer();
        } else {
            /* set high timeout */
            _parser.set_timeout(SPWF_READ_BIN_TIMEOUT);
            /* read in binary data */
            int read = _parser.read(buffer, amount);
            /* reset timeout value */
            _parser.set_timeout(_timeout);
            if(read > 0) {
                if(_recv_ok()) {
                    ret = amount;

                    /* remove from pending sizes
                     * (MUST be done before next async indications handling (e.g. `_winds_on()`)) */
                    _remove_pending_pkt_size(spwf_id, amount);
                } else {
                    debug_if(_dbg_on, "\r\nSPWF> failed to receive OK (%s, %d)\r\n", __func__, __LINE__);
                    empty_rx_buffer();
                }
            } else {
                debug_if(_dbg_on, "\r\nSPWF> failed to read binary data (%s, %d)\r\n", __func__, __LINE__);
                empty_rx_buffer();
            }
        }
    } else {
        debug_if(_dbg_on, "%s(%d): failed to send SOCKR\r\n", __func__, __LINE__);
    }

    debug_if(_dbg_on, "\r\nSPWF> %s():\t%d:%d\r\n", __func__, spwf_id, amount);

    /* unblock asynchronous indications */
    _winds_on();

    return ret;
}

/* betzw - TODO: improve performance! */
bool SPWFSA04::_recv_ap(nsapi_wifi_ap_t *ap)
{
    bool ret;
    int curr;
    unsigned int channel;
    int trials;

    ap->security = NSAPI_SECURITY_UNKNOWN;

    /* determine list end */
    curr = _parser.getc();
    if(curr == 'A') { // assume end of list ("AT-S.OK")
        if(!(_parser.recv("T-S.OK\n") && _recv_delim_lf())) {
            empty_rx_buffer();
        }
        return false;
    }

    /* run to 'horizontal tab' */
    trials = 0;
    while(_parser.getc() != '\x09') {
        if(trials++ > SPWFXX_MAX_TRIALS) {
            debug("\r\nSPWF> WARNING: might happen in case of RX buffer overflow! (%s, %d)\r\n", __func__, __LINE__);
            empty_rx_buffer();
            return false;
        }
    }

    /* read in next line */
    ret = _parser.recv("%255[^\n]\n", _msg_buffer) && _recv_delim_lf();

    /* parse line - first phase */
    if(ret) {
        int val = sscanf(_msg_buffer,
                         " %*s %hhx:%hhx:%hhx:%hhx:%hhx:%hhx CHAN: %u RSSI: %hhd SSID: \'%*255[^\']\'",
                         &ap->bssid[0], &ap->bssid[1], &ap->bssid[2], &ap->bssid[3], &ap->bssid[4], &ap->bssid[5],
                         &channel, &ap->rssi);
        if(val < 8) {
            ret = false;
        }
    }

    /* parse line - second phase */
    if(ret) { // ret == true
        char value;
        char *rest, *first, *last;

        /* decide about position of `CAPS:` */
        first = strchr(_msg_buffer, '\'');
        if(first == NULL) {
            debug("\r\nSPWF> WARNING: might happen in case of RX buffer overflow! (%s, %d)\r\n", __func__, __LINE__);
            empty_rx_buffer();
            return false;
        }
        last = strrchr(_msg_buffer, '\'');
        if((last == NULL) || (last < (first+1))) {
            debug("\r\nSPWF> WARNING: might happen in case of RX buffer overflow! (%s, %d)\r\n", __func__, __LINE__);
            empty_rx_buffer();
            return false;
        }
        rest = strstr(last, "CAPS:");
        if(rest == NULL) {
            debug("\r\nSPWF> WARNING: might happen in case of RX buffer overflow! (%s, %d)\r\n", __func__, __LINE__);
            empty_rx_buffer();
            return false;
        }

        /* substitute '\'' with '\0' */
        *last = '\0';

        /* copy values */
        memcpy(&ap->ssid, first+1, sizeof(ap->ssid)-1);
        ap->ssid[sizeof(ap->ssid)-1] = '\0';
        ap->channel = channel;

        /* skip `CAPS: 0421 ` */
        if(strlen(rest) < 11) {
            debug("\r\nSPWF> WARNING: might happen in case of RX buffer overflow! (%s, %d)\r\n", __func__, __LINE__);
            empty_rx_buffer();
            return false;
        }
        rest += 11;

        /* get next character */
        value = *rest++;
        if(value != 'W') { // no security
            ap->security = NSAPI_SECURITY_NONE;
            return true;
        }

        /* determine security */
        {
            char buffer[10];

            if(!(sscanf(rest, "%s%*[\x20]", (char*)&buffer) > 0)) { // '\0x20' == <space>
                return true;
            } else if(strncmp("EP", buffer, 10) == 0) {
                ap->security = NSAPI_SECURITY_WEP;
                return true;
            } else if(strncmp("PA2", buffer, 10) == 0) {
                ap->security = NSAPI_SECURITY_WPA2;
                return true;
            } else if(strncmp("PA", buffer, 10) != 0) {
                return true;
            }

            /* got a "WPA", check for "WPA2" */
            rest += strlen(buffer);
            value = *rest++;
            if(value == '\0') { // no further protocol
                ap->security = NSAPI_SECURITY_WPA;
                return true;
            } else { // assume "WPA2"
                ap->security = NSAPI_SECURITY_WPA_WPA2;
                return true;
            }
        }
    } else { // ret == false
        debug("\r\nSPWF> WARNING: might happen in case of RX buffer overflow! (%s, %d)\r\n", __func__, __LINE__);
        empty_rx_buffer();
    }

    return ret;
}

nsapi_size_or_error_t SPWFSA04::scan(WiFiAccessPoint *res, unsigned limit)
{
    unsigned int cnt = 0, found;
    nsapi_wifi_ap_t ap;

    if (!_parser.send("AT+S.SCAN=s,")) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    if(!(_parser.recv("AT-S.Parsing Networks:%u\n", &found) && _recv_delim_lf())) {
        debug_if(_dbg_on, "SPWF> error start network scanning\r\n");
        empty_rx_buffer();
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    debug_if(_dbg_on, "AT^ AT-S.Parsing Networks:%u\r\n", found);

    if(found > 0) {
        while (_recv_ap(&ap)) {
            if (cnt < limit) {
                res[cnt] = WiFiAccessPoint(ap);
            }

            if (!((limit != 0) && ((cnt + 1) > limit))) {
                cnt++;
            }
        }
    } else {
        if(!_recv_ok()) {
            empty_rx_buffer();
        }
    }

    return cnt;
}

#endif // MBED_CONF_IDW0XX1_EXPANSION_BOARD
