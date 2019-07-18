# Cnginx
A Nginx-like event-driven TCP networking framework
1) support creating one master process and multiple worker processes;
2) implement an event-driven approach with one event-loop per worker process to handle TCP networking;
3) apply epoll LT mode for data receiving and sending
4) apply multithreads / thread-pool to handle time-consuming work in worker process
