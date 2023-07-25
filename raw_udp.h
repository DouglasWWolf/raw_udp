#pragma once


class CRawUDP
{
public:

    void    connect_nic(const char* nic_name);

    void    make_header(const void* src_mac, const void* dst_mac, const void* src_ip, const void* dst_ip, int src_port, int dst_port);

    void    send(const char* str);

protected:

    // Socket descriptor
    int     m_sd;
    
    // Network interface index
    int     m_if_idx;

    // Buffer that contains the data to send over the socket
    unsigned char m_buffer[8192];
};