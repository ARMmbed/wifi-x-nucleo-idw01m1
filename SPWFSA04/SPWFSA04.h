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
 
#ifndef SPWFSA04_H
#define SPWFSA04_H

#include "mbed.h"
#include "ATParser.h"
#include "BlockExecuter.h"

#include "./spwfsa04_at_strings.h"
#include "../SPWFSAxx.h"

class SpwfSAInterface;

/** SPWFSA04 Interface class.
    This is an interface to a SPWFSA04 module.
 */
class SPWFSA04 : public SPWFSAxx
{
public:
    SPWFSA04(PinName tx, PinName rx,
             PinName rts, PinName cts,
             SpwfSAInterface &ifce, bool debug,
             PinName wakeup, PinName reset);

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

    /** Scan for available networks
     *
     * @param  ap    Pointer to allocated array to store discovered AP
     * @param  limit Size of allocated @a res array, or 0 to only count available AP
     * @return       Number of entries in @a res, or if @a count was 0 number of available networks, negative on error
     *               see @a nsapi_error
     */
    nsapi_size_or_error_t scan(WiFiAccessPoint *res, unsigned limit);

private:
    bool _recv_ap(nsapi_wifi_ap_t *ap);
    int _read_in(char*, int, uint32_t);
};

#endif // SPWFSA04_H
