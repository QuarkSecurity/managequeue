# managequeue
--------------

## Dependencies:
libconfig-devel


## Build
To build run `make`.
To clean run `make clean`.

## Running
./mqclient <send/receive> <path>
or
./mqclient <send/receive> -c <config file>

The send command reads data from stdin and sents it as a single message
to the message queue.

The receive command reads a single message from the message queue and writes
it to stdout.

## Example
echo hello | ./mqclient send /var/run/mq/example && ./mqclient receive /var/run/mq/example
