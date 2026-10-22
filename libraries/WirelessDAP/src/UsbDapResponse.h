#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef WD_USB_COMPAT
#define WD_USB_COMPAT 0
#endif
#ifndef WD_DAP128
#define WD_DAP128 0
#endif

// TinyUSB vendor is a byte stream, not a packet queue. Short responses queued
// behind a busy IN endpoint concatenate. The compatibility endpoint serializes
// completions (including full-packet ZLP); the original build uses padding.
#include "DapGuard.h"

template<class Endpoint>
bool sendDapUsbPacket(Endpoint& ep, const uint8_t* data, size_t length) {
  const size_t capacity=DapPacketSize;
  if (!length || length > capacity || !ep.mounted() || ep.available() < length) return false;
  uint8_t packet[DapPacketSize] = {};
  memcpy(packet, data, length);
  const size_t sentLength=WD_USB_COMPAT?length:sizeof(packet);
  if (ep.write(packet, sentLength) != sentLength) return false;
  ep.flush();
  return true;
}

