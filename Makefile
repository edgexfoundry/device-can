
.PHONY: build test clean docker

MICROSERVICES=build/release/device-can/device-can
.PHONY: $(MICROSERVICES)

DOCKERS=docker_device_can
.PHONY: $(DOCKERS)

VERSION=$(shell cat ./VERSION || echo 0.0.0)
GIT_SHA=$(shell git rev-parse HEAD)

build: ./VERSION ${MICROSERVICES}

build/release/device-can/device-can:
	    scripts/build.sh

test:
	    @echo $(MICROSERVICES)

clean:
	    rm -f $(MICROSERVICES)
	    rm -f ./VERSION

./VERSION:
	    @git describe --abbrev=0 | sed 's/^v//' > ./VERSION

version: ./VERSION
	    @echo ${VERSION}

docker: ./VERSION $(DOCKERS)

docker_device_can:
	    docker build \
	        -f scripts/Dockerfile.alpine \
	        --label "git_sha=$(GIT_SHA)" \
	        -t edgexfoundry/device-can:${GIT_SHA} \
	        -t edgexfoundry/device-can:${VERSION}-dev \
            .
