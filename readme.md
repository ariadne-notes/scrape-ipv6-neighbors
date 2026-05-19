Reads the raw NETLINK info from the container to get IPv6 neighbor information.

### Build on host
`gcc -static -o ndp_dump ndp_dump.c`

### Copy into container
`docker cp ndp_dump <container_name>:/usr/local/bin/ndp_dump`
