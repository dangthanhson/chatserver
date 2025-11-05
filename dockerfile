FROM ubuntu:22.04
RUN apt-get update && \
    apt-get install -y libstdc++6 && \
    apt-get clean

COPY build/meson-src/server /usr/local/bin/server
RUN chmod +x /usr/local/bin/server

EXPOSE 9090
ENTRYPOINT ["/usr/local/bin/server"]
