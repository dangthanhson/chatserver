# chatserver


Chat server implement with GRPC and Protobuf

This project use [Meson](https://mesonbuild.com) as build system,
While the dependency Grpc could be installed in the system, but that may cause incompatibility issue with the pre-gen proto file.
So [Conan](https://docs.conan.io/2/) was used as package manager, it will install the dependcies and make sure it is correct version.

```
sudo apt install meson
pip3 install conan
conan install . --output-folder=build --build=missing
cd build
meson setup --native-file conan_meson_native.ini .. meson-src
meson compile -C meson-src
```

The output binary will be
```
./build/meson-src/client
./build/meson-src/server
```

To build and run unit test
```
meson test -C meson-src
```

To build the docker image
```
docker build -t chatserver -f dockerfile .

docker run --rm -p 9090:9090 chatserver
```
Then call the client with
```
./build/meson-src/client NAME
```

To regenerate the proto files
```
PROTOC=`find ~/.conan2/ -wholename "*/bin/protoc" | head -n 1`
PROTOC_GEN_GRPC=`find ~/.conan2/ -name grpc_cpp_plugin | head -n 1`

$PROTOC --plugin=protoc-gen-grpc=$PROTOC_GEN_GRPC --grpc_out=generate_mock_code=true:. --cpp_out=. ./proto/chatservice.proto
```
