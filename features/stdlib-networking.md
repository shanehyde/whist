# Standard Library: Networking

TCP/UDP sockets, HTTP client, and URL parsing.

## Overview

| Module | Purpose |
|--------|---------|
| `net` | Low-level TCP/UDP sockets |
| `http` | HTTP client and server |
| `url` | URL parsing and construction |

## TCP Sockets (net module)

### TCP Client

```whist
import net;

// Connect to server
var stream = net.TcpStream::connect("example.com:80")?;

// Write request
stream.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")?;

// Read response
var response = stream.read_to_string()?;
print(response);

// Close (automatic on drop, but can be explicit)
stream.close();
```

### TCP Server

```whist
import net;

// Bind to address
var listener = net.TcpListener::bind("127.0.0.1:8080")?;
print("Listening on port 8080...\n");

// Accept connections
foreach stream in listener.incoming() {
    var client = stream?;
    print("Connection from {client.peer_addr()}\n");

    // Handle in new thread (if threading available)
    spawn {
        handle_client(client);
    };
}

func handle_client(stream: TcpStream) -> void {
    var request = stream.read_to_string()?;
    stream.write("HTTP/1.1 200 OK\r\n\r\nHello!")?;
}
```

### TCP API

```whist
struct TcpStream { ... }

impl TcpStream {
    // Connection
    func connect(addr: string) -> Result<TcpStream, NetError>;
    func connect_timeout(addr: string, timeout: Duration) -> Result<TcpStream, NetError>;

    // I/O (implements Read + Write)
    func read(buf: Span<u8>) -> Result<i64, NetError>;
    func write(data: Span<u8>) -> Result<i64, NetError>;
    func flush() -> Result<void, NetError>;

    // Info
    func peer_addr() -> SocketAddr;
    func local_addr() -> SocketAddr;

    // Options
    func set_read_timeout(timeout: ?Duration) -> Result<void, NetError>;
    func set_write_timeout(timeout: ?Duration) -> Result<void, NetError>;
    func set_nodelay(nodelay: bool) -> Result<void, NetError>;

    // Shutdown
    func shutdown(how: Shutdown) -> Result<void, NetError>;
    func close() -> void;
}

struct TcpListener { ... }

impl TcpListener {
    func bind(addr: string) -> Result<TcpListener, NetError>;
    func accept() -> Result<(TcpStream, SocketAddr), NetError>;
    func incoming() -> Iterator<Result<TcpStream, NetError>>;
    func local_addr() -> SocketAddr;
}

enum Shutdown {
    Read,
    Write,
    Both,
}
```

## UDP Sockets

```whist
import net;

// Create UDP socket
var socket = net.UdpSocket::bind("127.0.0.1:0")?;
print("Bound to {socket.local_addr()}\n");

// Send data
socket.send_to("Hello!", "127.0.0.1:8080")?;

// Receive data
var buf = [0u8; 1024];
var (len, addr) = socket.recv_from(buf)?;
print("Received {len} bytes from {addr}\n");

// Connected UDP (for repeated sends to same address)
socket.connect("127.0.0.1:8080")?;
socket.send("Hello!")?;
var data = socket.recv(buf)?;
```

### UDP API

```whist
struct UdpSocket { ... }

impl UdpSocket {
    func bind(addr: string) -> Result<UdpSocket, NetError>;

    // Unconnected
    func send_to(data: Span<u8>, addr: string) -> Result<i64, NetError>;
    func recv_from(buf: Span<u8>) -> Result<(i64, SocketAddr), NetError>;

    // Connected
    func connect(addr: string) -> Result<void, NetError>;
    func send(data: Span<u8>) -> Result<i64, NetError>;
    func recv(buf: Span<u8>) -> Result<i64, NetError>;

    // Info
    func local_addr() -> SocketAddr;
    func peer_addr() -> Result<SocketAddr, NetError>;

    // Options
    func set_broadcast(broadcast: bool) -> Result<void, NetError>;
    func set_ttl(ttl: i32) -> Result<void, NetError>;
}
```

## Socket Addresses

```whist
struct SocketAddr {
    ip: IpAddr,
    port: u16,
}

enum IpAddr {
    V4(Ipv4Addr),
    V6(Ipv6Addr),
}

struct Ipv4Addr {
    octets: [u8; 4],
}

struct Ipv6Addr {
    segments: [u16; 8],
}

// Parsing
var addr = SocketAddr::parse("127.0.0.1:8080")?;
var ip = IpAddr::parse("192.168.1.1")?;

// Construction
var addr = SocketAddr { ip: IpAddr::V4(Ipv4Addr::LOCALHOST), port: 80 };

// Constants
Ipv4Addr::LOCALHOST    // 127.0.0.1
Ipv4Addr::UNSPECIFIED  // 0.0.0.0
Ipv4Addr::BROADCAST    // 255.255.255.255
Ipv6Addr::LOCALHOST    // ::1
Ipv6Addr::UNSPECIFIED  // ::
```

## DNS Resolution

```whist
import net;

// Resolve hostname
var addrs = net.lookup_host("example.com")?;
foreach addr in addrs {
    print("{addr}\n");
}

// Reverse lookup
var hostname = net.lookup_addr("93.184.216.34")?;
```

## HTTP Client (http module)

### Simple Requests

```whist
import http;

// GET request
var response = http.get("https://api.example.com/data")?;
print("Status: {response.status}\n");
print("Body: {response.text()?}\n");

// POST with JSON
var response = http.post("https://api.example.com/users")
    .json(User { name: "Alice", age: 30 })?
    .send()?;

// With headers
var response = http.get("https://api.example.com/data")
    .header("Authorization", "Bearer token123")
    .header("Accept", "application/json")
    .send()?;
```

### Request Builder

```whist
var client = http.Client::new();

var response = client.request(Method::POST, "https://api.example.com/data")
    .header("Content-Type", "application/json")
    .body("{\"key\": \"value\"}")
    .timeout(Duration::seconds(30))
    .send()?;

// Reuse client for connection pooling
var resp1 = client.get("https://api.example.com/a").send()?;
var resp2 = client.get("https://api.example.com/b").send()?;
```

### HTTP API

```whist
// Convenience functions
func get(url: string) -> RequestBuilder;
func post(url: string) -> RequestBuilder;
func put(url: string) -> RequestBuilder;
func delete(url: string) -> RequestBuilder;
func head(url: string) -> RequestBuilder;

struct Client { ... }

impl Client {
    func new() -> Client;
    func with_config(config: ClientConfig) -> Client;

    func request(method: Method, url: string) -> RequestBuilder;
    func get(url: string) -> RequestBuilder;
    func post(url: string) -> RequestBuilder;
    // ...
}

struct RequestBuilder { ... }

impl RequestBuilder {
    func header(key: string, value: string) -> RequestBuilder;
    func headers(headers: HashMap<string, string>) -> RequestBuilder;
    func body(body: string) -> RequestBuilder;
    func json<T: Serialize>(value: T) -> Result<RequestBuilder, Error>;
    func form(data: HashMap<string, string>) -> RequestBuilder;
    func query(params: HashMap<string, string>) -> RequestBuilder;
    func timeout(timeout: Duration) -> RequestBuilder;
    func send() -> Result<Response, HttpError>;
}

struct Response { ... }

impl Response {
    func status() -> StatusCode;
    func headers() -> Headers;
    func text() -> Result<string, HttpError>;
    func bytes() -> Result<Vec<u8>, HttpError>;
    func json<T: Deserialize>() -> Result<T, HttpError>;
}

struct StatusCode {
    code: i32,
}

impl StatusCode {
    func is_success() -> bool;      // 200-299
    func is_redirect() -> bool;     // 300-399
    func is_client_error() -> bool; // 400-499
    func is_server_error() -> bool; // 500-599
}

enum Method {
    GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH,
}
```

### Client Configuration

```whist
var client = http.Client::with_config(ClientConfig {
    timeout: Duration::seconds(30),
    connect_timeout: Duration::seconds(10),
    pool_max_idle: 10,
    pool_idle_timeout: Duration::seconds(90),
    follow_redirects: true,
    max_redirects: 10,
    user_agent: "MyApp/1.0",
});
```

## URL Parsing (url module)

```whist
import url;

// Parse URL
var u = url.parse("https://user:pass@example.com:8080/path?query=value#fragment")?;

print("Scheme: {u.scheme}");      // https
print("Host: {u.host}");          // example.com
print("Port: {u.port}");          // 8080
print("Path: {u.path}");          // /path
print("Query: {u.query}");        // query=value
print("Fragment: {u.fragment}");  // fragment
print("Username: {u.username}");  // user
print("Password: {u.password}");  // pass

// Build URL
var u = url.Url::new()
    .scheme("https")
    .host("api.example.com")
    .path("/v1/users")
    .query_param("page", "1")
    .query_param("limit", "10")
    .build();

print(u.to_string());  // https://api.example.com/v1/users?page=1&limit=10

// Join paths
var base = url.parse("https://example.com/api/")?;
var full = base.join("users/123")?;  // https://example.com/api/users/123
```

### URL API

```whist
struct Url {
    scheme: string,
    username: ?string,
    password: ?string,
    host: ?string,
    port: ?u16,
    path: string,
    query: ?string,
    fragment: ?string,
}

impl Url {
    func parse(s: string) -> Result<Url, ParseError>;
    func to_string() -> string;
    func join(path: string) -> Result<Url, ParseError>;

    func query_pairs() -> Iterator<(string, string)>;
    func set_query_param(key: string, value: string) -> void;
}

// URL encoding
func encode(s: string) -> string;           // "hello world" -> "hello%20world"
func decode(s: string) -> Result<string, Error>;
func encode_component(s: string) -> string; // More aggressive encoding
```

## Error Types

```whist
enum NetError {
    ConnectionRefused,
    ConnectionReset,
    ConnectionAborted,
    NotConnected,
    AddrInUse,
    AddrNotAvailable,
    TimedOut,
    InvalidInput,
    InvalidData,
    Other(string),
}

enum HttpError {
    Network(NetError),
    Timeout,
    TooManyRedirects,
    InvalidUrl(string),
    InvalidHeader(string),
    InvalidBody(string),
    Status(StatusCode, string),
}
```

## Examples

### HTTP API Client

```whist
import http;
import json;

struct ApiClient {
    base_url: string,
    client: http.Client,
    token: ?string,
}

impl ApiClient {
    func new(base_url: string) -> ApiClient {
        return ApiClient {
            base_url: base_url,
            client: http.Client::new(),
            token: null,
        };
    }

    func (ApiClient) authenticate(username: string, password: string) -> Result<void, Error> {
        var response = self.client
            .post("{self.base_url}/auth/login")
            .json(LoginRequest { username, password })?
            .send()?;

        if !response.status().is_success() {
            return Err(Error::AuthFailed);
        }

        var data = response.json::<LoginResponse>()?;
        self.token = Some(data.token);
        return Ok(());
    }

    func (ApiClient) get_users() -> Result<Vec<User>, Error> {
        var req = self.client.get("{self.base_url}/users");

        if let Some(token) = self.token {
            req = req.header("Authorization", "Bearer {token}");
        }

        var response = req.send()?;
        return response.json::<Vec<User>>();
    }
}
```

### Simple Echo Server

```whist
import net;
import io;

func main() -> Result<void, Error> {
    var listener = net.TcpListener::bind("127.0.0.1:7878")?;
    print("Echo server listening on port 7878\n");

    foreach stream in listener.incoming() {
        var client = stream?;
        print("Client connected: {client.peer_addr()}\n");

        var reader = io.BufReader::new(client);
        foreach line in reader.lines() {
            var text = line?;
            print("Received: {text}\n");
            client.write("{text}\n")?;
        }

        print("Client disconnected\n");
    }

    return Ok(());
}
```

### URL Shortener Client

```whist
import http;
import url;

func shorten_url(long_url: string) -> Result<string, Error> {
    var response = http.post("https://api.short.io/links")
        .header("Authorization", "Bearer {API_KEY}")
        .json(CreateLinkRequest {
            originalURL: long_url,
            domain: "short.io",
        })?
        .send()?;

    if !response.status().is_success() {
        return Err(Error::ApiError(response.text()?));
    }

    var data = response.json::<CreateLinkResponse>()?;
    return Ok(data.shortURL);
}
```

## Open Questions

1. **Async networking?**
   - Blocking initially
   - Add async/await later with runtime

2. **TLS/SSL?**
   - Built-in or optional?
   - Which TLS library?

3. **WebSockets?**
   - Part of http module?
   - Separate module?

4. **HTTP/2 and HTTP/3?**
   - Start with HTTP/1.1
   - Add newer protocols later

5. **Connection pooling?**
   - Built into Client?
   - Manual management?

## Related Features

- [Result/Option](result-option.md) - Error handling
- [Traits](traits.md) - Read, Write for streams
- [String Interpolation](string-interpolation.md) - URL building
