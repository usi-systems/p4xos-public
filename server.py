#!/usr/bin/python
 
import socket
import sys
import struct
import binascii
 
multicast_group = '224.3.29.71'
HOST = ''   # Symbolic name meaning all available interfaces
PORT = 34952 # Arbitrary non-privileged port
 
# Datagram (udp) socket
def main():
    try :
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print 'Socket created'
    except socket.error, msg :
        print 'Failed to create socket. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
        sys.exit()
     
    # Bind socket to local host and port
    try:
        s.bind((HOST, PORT))
    except socket.error , msg:
        print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
        sys.exit()
         
    try:
        group = socket.inet_aton(multicast_group)
        mreq  = struct.pack('4sL', group, socket.INADDR_ANY)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    except socket.error , msg:
        print 'Multicast . Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
        sys.exit()

    print 'Socket bind complete'
     
    packer = struct.Struct('>' + 'b h b b 3s')
    #now keep talking with the client
    while 1:
        # receive data from client (data, addr)
        d = s.recvfrom(1024)
        data = d[0]
        addr = d[1]

        print 'received "%s"' % binascii.hexlify(data)
        
        unpacked_data = packer.unpack(data)
        print 'unpacked:', unpacked_data
         
        if not data: 
            break
         
        typ, inst, rnd, vrnd,  msg = unpacked_data 
        print 'Message[%s:%d] - %d, %d, %d, %d, %s' % (addr[0], addr[1], typ, inst, rnd, vrnd, msg)
         
    s.close()

if __name__ == '__main__':
    main()
