#define HOST_TEST 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "EsbTransport.h"

int main() {
  printf("Starting AFH (Adaptive Frequency Hopping) unit tests...\n");

  EsbTransport transport;
  
  // Test 1: Channel 80 hopping table generation
  transport.setChannel(80);
  assert(transport.channel() == 80);
  const uint8_t* hops = transport.hopChannels();
  assert(hops[0] == 80);
  assert(hops[1] == 19);
  assert(hops[2] == 40);
  assert(hops[3] == 60);
  printf("AFH hop channels for CH80: [%u, %u, %u, %u] - PASS\n",
         hops[0], hops[1], hops[2], hops[3]);

  // Test 2: Channel 40 hopping table generation
  transport.setChannel(40);
  const uint8_t* hops40 = transport.hopChannels();
  assert(hops40[0] == 40);
  assert(hops40[1] == 60);
  assert(hops40[2] == 0);
  assert(hops40[3] == 20);
  printf("AFH hop channels for CH40: [%u, %u, %u, %u] - PASS\n",
         hops40[0], hops40[1], hops40[2], hops40[3]);

  // Test 2b: Mathematical verification: for ALL 81 base channels, verify all 4 hops are in [0, 80] and unique
  for (uint8_t c = 0; c <= 80; c++) {
    transport.setChannel(c);
    const uint8_t* h = transport.hopChannels();
    for (int i = 0; i < 4; i++) {
      assert(h[i] <= 80);
      for (int j = i + 1; j < 4; j++) {
        assert(h[i] != h[j]); // Must be strictly unique!
      }
    }
  }
  printf("AFH 100%% unique & valid channel generation verified for ALL base channels 0..80! - PASS\n");

  // Test 3: Channel cycling
  transport.setChannel(80);
  assert(transport.currentChannel() == 80);
  assert(transport.currentHopIndex() == 0);
  assert(transport.hopCount() == 0);

  transport.advanceHopChannel();
  assert(transport.currentChannel() == 19);
  assert(transport.currentHopIndex() == 1);
  assert(transport.hopCount() == 1);

  transport.advanceHopChannel();
  assert(transport.currentChannel() == 40);
  assert(transport.currentHopIndex() == 2);
  assert(transport.hopCount() == 2);

  transport.advanceHopChannel();
  assert(transport.currentChannel() == 60);
  assert(transport.currentHopIndex() == 3);
  assert(transport.hopCount() == 3);

  transport.advanceHopChannel();
  assert(transport.currentChannel() == 80);
  assert(transport.currentHopIndex() == 0);
  assert(transport.hopCount() == 4);
  printf("AFH channel advance & hop count - PASS\n");

  // Test 4: Disable AFH
  transport.setAfhEnabled(false);
  transport.advanceHopChannel();
  assert(transport.currentChannel() == 80); // Should not advance when disabled
  assert(transport.hopCount() == 4);
  printf("AFH disable toggle - PASS\n");

  printf("All AFH unit tests PASSED successfully!\n");
  return 0;
}
