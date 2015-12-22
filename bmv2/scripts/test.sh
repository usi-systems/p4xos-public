#!/bin/bash


for i in {1..1}
do
    curl -X POST http://10.0.0.1:8080/put -d key="ciao$i" -d value="hello$i" &
    curl -X GET  http://10.0.0.1:8080/get?key="ciao$i" &
    curl -X GET  http://10.0.0.1:8080/get?key=nonexist &
done

wait