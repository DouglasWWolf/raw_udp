#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include <linux/if_packet.h>
#include "raw_udp.h"
#include "endian_types.h"

#pragma pack(push, 1)

struct eth_hdr_t
{
    uint8_t     dst_mac[6];
    uint8_t     src_mac[6];
    be_uint16_t frame_type;
};

struct ipv4_hdr_t
{
    uint8_t     version;
    uint8_t     dsf;
    be_uint16_t length;
    be_uint16_t id;
    be_uint16_t flags;
    uint8_t     time_to_live;
    uint8_t     protocol;
    be_uint16_t checksum;
    uint8_t     src_ip[4];
    uint8_t     dst_ip[4];
};

struct udp_hdr_t
{
    be_uint16_t    src_port;
    be_uint16_t    dst_port;
    be_uint16_t    length;
    be_uint16_t    checksum;
};

struct raw_udp_t
{
    eth_hdr_t   eth;
    ipv4_hdr_t  ipv4;
    udp_hdr_t   udp;
    uint8_t     payload[8000];
};
#pragma pack(pop)

void dump(void* data, int length)
{

    int n=0;

    // Get a handy pointer to the data
    const unsigned char* p = (const unsigned char*)data;

    while (length--)
    {
        printf("%02X ", *p++);

        ++n;
        if (n%8 == 0) printf("  ");
        if (n%16 == 0) printf("\n");

    }
    printf("\n");

}


//=========================================================================================================
// connect_nic() - Opens the raw socket and fetches the index of the specific network interface
//=========================================================================================================
void CRawUDP::connect_nic(const char* nic_name)
{
    // Open raw socket to send on
	m_sd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);

    // If the open failed, barf
    if (m_sd == -1)
    {
	    perror("socket");
        exit(1);
	}

    struct ifreq if_data;

  	// Clear the structure that will hold our network interface information
    memset(&if_data, 0, sizeof(if_data));
	
    // Stuff the network interface name into the network interface info structure
    strncpy(if_data.ifr_name, nic_name, IFNAMSIZ-1);
	
    // Fetch information about this network interface
    if (ioctl(m_sd, SIOCGIFINDEX, &if_data) < 0)
    {
        perror("SIOCGIFINDEX");
        exit(1);
    }

    // And save the index of the user-specified network interface
    m_if_idx = if_data.ifr_ifindex;
}
//=========================================================================================================


void CRawUDP::make_header
(
    const void* src_mac,  const void* dst_mac,
    const void* src_ip,   const void* dst_ip,
    int         src_port, int         dst_port
)
{
    // Impose a raw_udp_t structure on the buffer
    raw_udp_t& frame = *(raw_udp_t*)m_buffer;    

    // Clear the entire buffer to 0xDD;
    memset(m_buffer, 0xDD, sizeof(m_buffer));

    // Fill in the Ethernet header
    memcpy(frame.eth.dst_mac, dst_mac, 6);
    memcpy(frame.eth.src_mac, src_mac, 6);
    frame.eth.frame_type = 0x0800;  /* 0x0800 = IPv4 */

    // Fill in the IPv4 Header
    frame.ipv4.version      = 0x45;     /* 0x45 = Standard IPv4 */
    frame.ipv4.dsf          = 0;
    frame.ipv4.id           = 0xABCD;  //???????????????  FILL THIS IN!!!!!!!!!!!!!!!!!!!!!!!!
    frame.ipv4.flags        = 0x4000;
    frame.ipv4.time_to_live = 0x40;
    frame.ipv4.protocol     = 0x11;     /* 0x11 = UDP */
    memcpy(frame.ipv4.src_ip, src_ip, 4);
    memcpy(frame.ipv4.dst_ip, dst_ip, 4);


    // Fill in the UDP header
    frame.udp.src_port = src_port;
    frame.udp.dst_port = dst_port;

}

static uint16_t ipv4_checksum(ipv4_hdr_t& header)
{
    uint16_t correction;
    uint32_t checksum = 0;

    header.checksum = 0;

    uint16_t* p = (uint16_t*)&header;
    for (int i=0; i<10; ++i)
    {
        uint16_t value = (*p << 8) | (*p >> 8);
        checksum += value;
        ++p;
    }

    correction = checksum >> 16;
    checksum &= 0xFFFF;
    checksum += correction;

    correction = checksum >> 16;
    checksum &= 0xFFFF;
    checksum += correction;
    
    return ~checksum;
}


void CRawUDP::send(const char* payload)
{
    int payload_size= strlen(payload);

    // Impose a raw_udp_t structure on the buffer
    raw_udp_t& frame = *(raw_udp_t*)m_buffer;    

    // Tell the user how many bytes of overhead are imposed on a UDP packet    
    printf("UDP overhead = %lu bytes\n", sizeof(frame) - sizeof(frame.payload));

    // Copy the UDP payload into the frame buffer
    memcpy(frame.payload, payload, payload_size);

    // Fill in the length of the IPv4 packet
    frame.ipv4.length = sizeof(ipv4_hdr_t) + sizeof(udp_hdr_t) + payload_size;
    frame.ipv4.checksum = ipv4_checksum(frame.ipv4);

    // Fill in the length of the UDP packet
    frame.udp.length = sizeof(udp_hdr_t) + payload_size;
    frame.udp.checksum = 0x0000;

    // Compute the number of bytes in the frame
    int frame_length = sizeof(frame) - sizeof(frame.payload) + payload_size;

    // Hexdump the frame data
    dump(&frame, frame_length);

    struct sockaddr_ll socket_address;

    // Fill in the interface index of the socket address
    socket_address.sll_ifindex = m_if_idx;

    // Fill in the length of the destination MAC
	socket_address.sll_halen = 6;

    // Fill in the destination MAC
    memcpy(socket_address.sll_addr, frame.eth.dst_mac, 6);

    // Send the packet to the network interface
    int rc = sendto(m_sd, &frame, frame_length, 0, (sockaddr*)&socket_address, sizeof(socket_address));

    if (rc < 1)
    {
        printf("sendto failed\n");        
    }

}