## compeer

Decentralised peer to peer messaging

### Generating key pair 
Use openssl to generate key pair for encrypted communication:

 ```
 cd ~/.ssh
 openssl genrsa -out private.pem 2048
 openssl rsa -in private.pem -outform PEM -pubout -out public.pem
 ```
