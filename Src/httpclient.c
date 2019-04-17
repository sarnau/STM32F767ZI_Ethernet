/*
	HTTP CLIENT FOR RAW LWIP
	(c) 2008-2009 Noyens Kenneth
	PUBLIC VERSION V0.2 16/05/2009

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE Version 2.1 as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	
*/
#include "httpclient.h"
#include <stdlib.h>
#include <string.h>

// Close a PCB(connection)
static void hc_clearpcb(struct tcp_pcb *pcb) {
  if (pcb != NULL) {
    tcp_close(pcb);
  }
}

// Function that lwip calls for handling recv'd data
static err_t hc_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
  struct hc_state *state = arg;

  if (err == ERR_OK) {
    if (p != NULL) {
      tcp_recved(pcb, p->tot_len);

      // Add payload (p) to state
      while (p != NULL) {
        (*state->ReturnPage)(state->Num, OK, p->payload, p->len);
        struct pbuf *next = p->next;
        pbuf_free(p);
        p = next;
      }
    } else { // NULL packet == CONNECTION IS CLOSED
      (*state->ReturnPage)(state->Num, OK, NULL, 0);
      hc_clearpcb(pcb);
      free(state);
    }
  }
  return ERR_OK;
}

// Function that lwip calls when there is an error
static void hc_error(void *arg, err_t err) {
  struct hc_state *state = arg;
  // pcb already deallocated

  (*state->ReturnPage)(state->Num, GEN_ERROR, NULL, 0);
  free(state);
}

// Function that lwip calls when the connection is idle
// Here we can kill connections that have stayed idle for too long
static err_t hc_poll(void *arg, struct tcp_pcb *pcb) {
  struct hc_state *state = arg;

  state->ConnectionTimeout++;
  if (state->ConnectionTimeout > 20) {
    tcp_abort(pcb); // Close the connection
    (*state->ReturnPage)(state->Num, TIMEOUT, NULL, 0);
  }
  return ERR_OK;
}

// lwip calls this function when the remote host has successfully received data (ack)
static err_t hc_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
  struct hc_state *state = arg;
  state->ConnectionTimeout = 0;
  return ERR_OK;
}

// lwip calls this function when the connection is established
static err_t hc_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
  struct hc_state *state = arg;

  if (err != ERR_OK) {
    hc_clearpcb(pcb);

    (*state->ReturnPage)(state->Num, GEN_ERROR, NULL, 0);
    free(state->RequestStr);
    free(state);
    return ERR_OK;
  }

  tcp_recv(pcb, hc_recv);
  tcp_err(pcb, hc_error);
  tcp_poll(pcb, hc_poll, 10);
  tcp_sent(pcb, hc_sent);

  tcp_write(pcb, state->RequestStr, strlen(state->RequestStr), 1);
  tcp_output(pcb);
  free(state->RequestStr);
  return ERR_OK;
}

// Public function for request a webpage (REMOTEIP, ...
int hc_open(ip_addr_t remoteIP, const char *requestStr, void (*returnpage)(u8_t, hc_errormsg, char *, u16_t)) {
  static u8_t num = 0;
  err_t err;

  struct tcp_pcb *pcb = tcp_new();
  if (pcb == NULL)
    return 0;
  struct hc_state *state = malloc(sizeof(struct hc_state));
  if (state == NULL) {
    tcp_close(pcb);
    return 0;
  }
  tcp_arg(pcb, state);

  // Define webclient state vars
  state->Num = ++num;
  state->ConnectionTimeout = 0;
  state->ReturnPage = returnpage;

  // Make place for PostVars & Page
  state->RequestStr = strdup(requestStr);
  if (state->RequestStr == NULL) {
    tcp_close(pcb);
    free(state);
    return 0;
  }

  // Bind to local IP & local port
//  ip_addr_t ipaddr = IPADDR4_INIT_BYTES(192, 168, 178, 69);
//  ip_addr_set(&pcb->local_ip, &ipaddr);
  err = tcp_bind(pcb, IP_ADDR_ANY, 0);
  if(err != ERR_OK)
    debug_printf("tcp_bind %s\n", lwip_strerr(err));

  err = tcp_connect(pcb, &remoteIP, 80, hc_connected);
  if(err != ERR_OK) {
    debug_printf("tcp_connect %s\n", lwip_strerr(err));
    return err;
  }
  return num;
}