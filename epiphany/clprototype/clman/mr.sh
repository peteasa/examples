#!/bin/sh

## Start the server with text in a file 49008-0.txt to read
nohup ./wordc -p 49008-0.txt -e 'tcp://*:31010' -t zmq.PAIR &

## Start the clients
#./clman ./wordc -m 'tcp://$clmanagerip$:31001' -e 'tcp://$peerip$:31010' -t zmq.PAIR
./clman ./run_wordc.sh 'tcp://192.169.0.118:31010'
