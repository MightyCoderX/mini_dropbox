FROM ubuntu:26.04

ENV ASAN_OPTIONS=abort_on_error=1:disable_coredump=0
ENV UBSAN_OPTIONS=abort_on_error=1

RUN apt-get update && \
    apt-get install -y --no-install-recommends libasan6 libubsan1 && \
    rm -rf /var/lib/apt/lists

