#!/bin/bash

DIR="certs"
mkdir -p $DIR

openssl genrsa -out $DIR/ca.key 4096
openssl req -x509 -new -nodes -key $DIR/ca.key -sha256 -days 365 \
    -out $DIR/ca.crt -subj "/CN=AccelRoot-CA"

openssl genrsa -out $DIR/server.key 2048
openssl req -new -key $DIR/server.key -out $DIR/server.csr -subj "/CN=localhost"
openssl x509 -req -in $DIR/server.csr -CA $DIR/ca.crt -CAkey $DIR/ca.key \
    -CAcreateserial -out $DIR/server.crt -days 365 -sha256

openssl genrsa -out $DIR/client.key 2048
openssl req -new -key $DIR/client.key -out $DIR/client.csr -subj "/CN=client"
openssl x509 -req -in $DIR/client.csr -CA $DIR/ca.crt -CAkey $DIR/ca.key \
    -CAcreateserial -out $DIR/client.crt -days 365 -sha256

rm $DIR/*.csr $DIR/*.srl
