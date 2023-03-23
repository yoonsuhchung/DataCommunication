import socket
import sys

maxbuf = 1024
port = 50000
goodbye_msg = 'Echo_CLOSE'


def terminate(data, csock, ssock):
    if data.find(goodbye_msg) >= 0:
        csock.send(data.encode())
        print(goodbye_msg)
        csock.close()
        ssock.close()
        sys.exit()


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s_sock:
    s_sock.bind(("localhost", port))
    s_sock.listen()
    print('Server socket listening')
    c_sock, address = s_sock.accept()
    print('Connection accepted')

    while True:
        data_in = c_sock.recv(maxbuf)
        terminate(data_in.decode(), c_sock, s_sock)
        if data_in.decode().startswith('SEND'):
            while not data_in.decode().endswith('RECV'):
                new_data = c_sock.recv(maxbuf)
                terminate(new_data.decode(), c_sock, s_sock)
                data_in += new_data
            data_in = data_in.decode()[4:-5].split('\n')
            for elem in data_in[:10]:
                print(elem)
                c_sock.send((elem+'\n').encode())





