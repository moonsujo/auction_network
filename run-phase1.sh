#!/bin/bash
echo "Starting server..."
./server.out > server.log 2>&1 &
pid_server=$!

echo "Starting seller 1..."
./seller.out items-seller1.txt > seller1.log 2>&1 &
pid_seller1=$!

echo "Starting seller 2..."
./seller.out items-seller2.txt > seller2.log 2>&1 &
pid_seller2=$!

echo "Sleeping for 5 seconds..."
sleep 5

echo "Terminating programs..."
kill -9 $pid_seller1
kill -9 $pid_seller2
kill -9 $pid_server

echo "Done."