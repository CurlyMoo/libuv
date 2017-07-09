/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"

#ifdef __linux__

#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <net/if_arp.h>
#include <ifaddrs.h>

TEST_IMPL(interface_address) {
  struct ifreq ifr;
  struct sockaddr_in *addr = NULL;
  uv_interface_address_t *interfaces = NULL;
  int fd = 0, s = 0, count = 0;
  int has_tap = 0, i = 0;
  char ip[17];
  char *clonedev = "/dev/net/tun";

  memset(&ifr, 0, sizeof(ifr));

  /*
   * Create tap device
   */
  fd = open(clonedev, O_RDWR);
  ASSERT(0 < fd);

  ifr.ifr_flags = IFF_TAP;
  strncpy(ifr.ifr_name, "tap0", IFNAMSIZ);

  ASSERT(0 == ioctl(fd, TUNSETIFF, (void *) &ifr));

  ASSERT(0 == strcmp("tap0", ifr.ifr_name));

  /*
   * Unregister the existing tap0 device
   */
  ASSERT(0 == ioctl(fd, TUNSETPERSIST, 0));
  /*
   * Register the existing tap0 device
   */
  ASSERT(0 == ioctl(fd, TUNSETPERSIST, 1));

  /*
   * Open a new socket so we can set
   * the ip and mac address of the
   * tap0 device
   */
  s = socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT(0 < s);

  memset(&ifr, 0, sizeof(ifr));

  strcpy(ifr.ifr_name, "tap0");
  memset(&ifr.ifr_hwaddr.sa_data[0], 0xAA, 1);
  memset(&ifr.ifr_hwaddr.sa_data[1], 0xBB, 1);
  memset(&ifr.ifr_hwaddr.sa_data[2], 0xCC, 1);
  memset(&ifr.ifr_hwaddr.sa_data[3], 0xDD, 1);
  memset(&ifr.ifr_hwaddr.sa_data[4], 0xEE, 1);
  memset(&ifr.ifr_hwaddr.sa_data[5], 0xFF, 1);
  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
  ASSERT(0 == ioctl(s, SIOCSIFHWADDR, &ifr));

  ifr.ifr_addr.sa_family = AF_INET;

  /*
   * Set the tap device ip address
   */
  addr = (struct sockaddr_in *)&ifr.ifr_addr;
  inet_pton(AF_INET, "10.0.1.2", &addr->sin_addr);
  ASSERT(0 == ioctl(s, SIOCSIFADDR, &ifr));

  /*
   * Set the tap device netmask
   */
  inet_pton(AF_INET, "255.255.0.0", ifr.ifr_addr.sa_data + 2);
  ASSERT(0 == ioctl(s, SIOCSIFNETMASK, &ifr));

  /*
   * Make sure the tap device is up and running
   */
  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  ASSERT(0 == ioctl(s, SIOCSIFFLAGS, &ifr));

  /*
   * We need to call a read to actually make
   * the system know the tap device is running
   */
  ASSERT(0 == read(fd, NULL, 0));

  ASSERT(0 == uv_interface_addresses(&interfaces, &count));
  ASSERT(0 < count);
  ASSERT(NULL != interfaces);

  for(i=0;i<count;i++) {
    if(strcmp(interfaces[i].name, "tap0") == 0) {
      ASSERT(AF_INET == interfaces[i].address.address4.sin_family);
      uv_ip4_name(&interfaces[i].address.address4, ip, sizeof(ip));
      ASSERT(0 == strcmp(ip, "10.0.1.2"));

      uv_ip4_name(&interfaces[i].netmask.netmask4, ip, sizeof(ip));
      ASSERT(0 == strcmp(ip, "255.255.0.0"));

      ASSERT(0xAA == (interfaces[i].phys_addr[0] & 0xFF));
      ASSERT(0xBB == (interfaces[i].phys_addr[1] & 0xFF));
      ASSERT(0xCC == (interfaces[i].phys_addr[2] & 0xFF));
      ASSERT(0xDD == (interfaces[i].phys_addr[3] & 0xFF));
      ASSERT(0xEE == (interfaces[i].phys_addr[4] & 0xFF));
      ASSERT(0xFF == (interfaces[i].phys_addr[5] & 0xFF));

      has_tap = 1;
      break;
    }
  }

  uv_free_interface_addresses(interfaces, count);

  ASSERT(1 == has_tap);

  close(s);
  /*
   * Remove the tap device again
   */
  ASSERT(0 == ioctl(fd, TUNSETPERSIST, 0));

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif