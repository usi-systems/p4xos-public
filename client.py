#!/usr/bin/python
import socket
import sys
import struct
import binascii
import argparse
 
multicast_group = ('224.3.29.71', 34952)

def main():
    parser = argparse.ArgumentParser(description='Generate Paxos messages.')
    parser.add_argument('--dst', default='10.0.0.1')
    parser.add_argument('--itf', default='eth0')
    parser.add_argument('--typ', type=int, default=1)
    parser.add_argument('--inst', type=int, default=1)
    parser.add_argument('--rnd', type=int, default=1)
    parser.add_argument('--vrnd', type=int, default=1)
    parser.add_argument('--value', default='Ciao')
    args = parser.parse_args()
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        ttl = struct.pack('b', 1)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
    except socket.error:
        print 'Failed to create socket'
        sys.exit()

    values = (args.typ, args.inst, args.rnd, args.vrnd, args.value)
    packer = struct.Struct('>' + 'b h b b 4s')
    packed_data = packer.pack(*values)
    try:
        print 'Original values:', values
        print 'Format string  :', packer.format
        print 'Uses           :', packer.size, 'bytes'
        print 'Packed Value   :', binascii.hexlify(packed_data)
        s.sendto(packed_data, multicast_group)
         
    except socket.error, msg:
        print 'Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
        sys.exit()

    finally:
        print >>sys.stderr, 'closing socket'
        s.close()

if __name__ == '__main__':
    main()
