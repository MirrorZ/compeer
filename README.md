## compeer

Decentralised peer to peer messenger

### TODO

~~Handle multiple messages correctly~~

~~Handle disconnection~~
* Add Signal Handlers 
* Enable Encryption


### Generating key pair 
 Use openssl to generate key pair for encrypted communication:

 ```
 cd ~/.ssh
 openssl genrsa -out private.pem 2048
 openssl rsa -in private.pem -outform PEM -pubout -out public.pem
 ```

### Suggestions/Issues

Feel free to add issues, submit patches.
Ideas are welcome!

Contact:

chandrika3437@gmail.com

ashmew2@gmail.com

