#!/bin/sh
curl -X POST http://10.0.0.1:8080/put -d key=ciao -d value=hello
curl -X GET  http://10.0.0.1:8080/get?key=ciao
curl -X GET  http://10.0.0.1:8080/get?key=notexist
