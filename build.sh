#!/bin/bash -x

AVR_CONTAINER_VERSION="latest"



function init() {

  DEPENDENCY_LABEL=`env | grep GO_DEPENDENCY_LABEL_ | cut -d= -f2 | sed 's/\s//g' | tr -d '\n'`


  if [ -z ${DEPENDENCY_LABEL} ]; then
    AVR_CONTAINER_VERSION="latest"
  else
    AVR_CONTAINER_VERSION="v${DEPENDENCY_LABEL}"
  fi


}


function build_software() {
  mkdir -p build;
  docker run --rm --user `id -u`:`id -g` -v $PWD:/build --entrypoint /build/build.sh ubirch/avr-build:${MAVEN_CONTAINER_VERSION}
  if [ $? -ne 0 ]; then
      echo "Docker build failed"
      exit 1
  fi
}

case "$1" in
    build)
        init
        build_software
        ;;
    *)
        echo "Usage: $0 {build|publish}"
        exit 1
esac

exit 0
