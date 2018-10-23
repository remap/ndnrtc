# `ndnrtc-client` Docker image

## Build image
* Build image
```
docker build -t ndnrtc-client .
```

* Build particular branch
```
docker build -t ndnrtc-client --build-arg BRANCH=dev .
```

## Run container
### Start producer

```
mkdir container-tmp
docker network create -d bridge ndnrtc-subnet
docker run --rm -ti --name ndnrtc-producer \
                    --network ndnrtc-subnet \
                    -v $(pwd)/container-tmp:/tmp \
                    ndnrtc-client
```

Image’s entry point is `run.sh` script. This script starts NFD in the background and then starts `ndnrtc-client` with sample producer configuration ([sample-producer.cfg](../cpp/tests/sample-producer.cfg)) in the foreground.

Image configured to store all generated files into container’s `tmp` folder. If you want to inspect any of those files, mount container’s `tmp` folder into a folder in your host machine. The command above mounts container’s `tmp` into `container-tmp` folder on the host machine.

You can provide environment variables, most of them are passed as arguments for `ndnrtc-client` (more information on command-line arguments can be found [here](../cpp/client#command-line-arguments). For a list of available variables, inspect [Dockerfile’s `ENV` entries](Dockerfile#L63).

### Start consumer
For consumer container to successfully fetch data from the producer container, first it needs to register NFD routes. Thus, it’s a two step process.

1. Start NFD and register route(s) towards producer container:
```
docker run --rm -d --name ndnrtc-consumer \
                   --network ndnrtc-subnet \
                   -v $(pwd)/container-tmp:/tmp \
                   -e START_CLIENT=no \
                   -e NFD_LOG=/tmp/nfd-consumer.log \
                   -e CONFIG_FILE=tests/sample-consumer.cfg \
                   ndnrtc-client
docker exec ndnrtc-consumer nfdc face create udp://ndnrtc-producer
docker exec ndnrtc-consumer nfdc route add /ndnrtc-test <face-id>
```

By passing `START_CLIENT=no` environment variable we prevent `run.sh` from starting `ndnrtc-client` yet. Instead, it will start only NFD in foreground.
> `<face-id>` can be obtained by examining output of `nfdc face create` command or  `nfd-status`:  
> ```  
> docker exec ndnrtc-consumer nfd-status  
> ```  

2. Start `ndnrtc-client` in running consumer container:
```
docker exec -ti ndnrtc-consumer bash -c "export START_CLIENT=yes &&./run.sh"
```

Inspect `container-tmp` for generated files. More information on generated files [here](../cpp/client).
