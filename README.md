## compeer

Decentralised peer to peer messaging

### Compiling
on Ubuntu, libssl-dev is required to build.
For other systems, find a similar package, (openssl/rsa.h)

Compile with: g++ crypto.cpp peer.cpp -lcrypto

---

### Generating key pair
Use openssl to generate key pair for encrypted communication:

 ```
 cd ~/.ssh
 openssl genrsa -out private.pem 2048
 openssl rsa -in private.pem -outform PEM -pubout -out public.pem
 ```
