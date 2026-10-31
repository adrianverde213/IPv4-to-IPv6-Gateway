# Dual-Stack-Gateway-with-Packet-Manipulation-

# Dual-Stack Gateway (IPv4 → IPv6)

Academic mini-project for the *Internet Protocols* course (Technical University of Cluj-Napoca, 2026).

A concurrent TCP server written in C for Linux that acts as a gateway between an IPv4 client and an IPv6 HTTP server.

## How it works

1. The server listens on IPv4 TCP port `22227` on all network interfaces.
2. Each accepted client is handled in a separate child process created with `fork()`, which keeps memory isolated between clients while the parent keeps accepting connections.
3. The server receives a command in the form `xy#` from the client (a Windows IPv4 client application provided by the course).
4. For command `07#`, the server acts as an IPv6 client: it resolves the host with `getaddrinfo()` (IPv6 only), connects over TCP on port 80 and sends `GET / HTTP/1.0\r\n\r\n`.
5. The HTTP response is received and the header is removed by locating the `\r\n\r\n` delimiter. Carriage returns are stripped, the page is saved locally as `index.html` and the content is forwarded to the IPv4 client.
6. Any other command receives the reply `Comanda neimplementata` ("command not implemented").

Return values of the socket functions are checked and reported.

## Build and run

Requires Linux with IPv6 connectivity and `gcc`.

    gcc dual_stack_gateway.c -o gateway
    ./gateway

## Testing

Tested with the Windows client provided by the course, configured to connect to the server on port 22227 and send command `07#`. The received page was displayed by the client and saved on the server as `index.html` (viewed with `lynx`).

## Limitations

- Only command `07#` is implemented. The target host is hardcoded to `ipv6.test-ipv6.com`, because the host originally assigned for this command was unreachable.
- The Windows client used for testing is provided by the course and is not included in this repository.

## Concepts demonstrated

- TCP sockets over IPv4 and IPv6 (`socket`, `bind`, `listen`, `accept`, `connect`)
- Name resolution with `getaddrinfo()`
- Process-based concurrency with `fork()`
- Manual parsing of an HTTP/1.0 response
