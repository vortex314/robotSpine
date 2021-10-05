# Why Redis
- single threaded, no context switch overhead on latency
- swiss army knife, very versatile, contains all features I needed : Pub/Sub, time series, streams 
- fast
- can be used remotely 
- integrates with a lot of stuff : NodeJS, NodeRed, Grafana, Elastic & Kibana
But...
- it uses TCP with connection establishment overhead, latency 
- text based interface to engine 
- array of arrays in reply , complex to decode
# Issue with iceoryx
Iceoryx would have been a nice implementation , it's extremely fast ( < 1 msec latency ) and gas a 3 level publish and subscription pattern, just like my own.
- C++ constructs complex to use
- Wildcard Subscription was abandoned 

# Issue with Zenoh
Zenoh looks to address the data in motion and data at rest pattern that I needed for my robot firmware. 
The protocol looks highely optimized for low bandwidth communication, needs some sharing of ID's 

But ...
- it's so new it lacks integration with a lot of other products 
- it's written in RUST and cannot be compiled on a Raspberry Pi 
- It uses a homongoneous number of threads which is basically overkill on a poor Raspberry pi 
- being developed by a small team ( +/- 3 )

# Build my own ?
- IPC shared memory interface
- CBOR API
- Pub/sub wildcard, time series, streams
- UDP, Serial & TCP 
- in memory caching
- single threaded
- C++