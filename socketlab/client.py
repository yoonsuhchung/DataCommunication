import socket
import sys
import queue

maxbuf = 1024
port = 50000
goodbye_msg = 'Echo_CLOSE'


def pending(sock):
    msg = sock.recv(maxbuf).decode()
    while msg.find(goodbye_msg) == -1:
        msg += sock.recv(maxbuf).decode()
    sock.close()
    print(goodbye_msg)
    sys.exit()


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as c_sock:
    q = queue.Queue()
    c_sock.connect(("localhost", port))
    print('Client socket connected')
    while True:
        while True:
            data = input()
            if data == 'Q':
                break
            if data == 'bye':
                c_sock.send(goodbye_msg.encode())
                pending(c_sock)
            q.put(data)
        c_sock.send('SEND'.encode())
        print('SEND')
        while not q.empty():
            data_out = q.get()
            c_sock.send((data_out+'\n').encode())
            print(data_out)
        c_sock.send('RECV'.encode())
        print('RECV')



