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

echo "Starting buyer 1..."
./buyer.out items-buyer1.txt > buyer1.log 2>&1 & 
pid_buyer1=$!

echo "Starting buyer 2..."
./buyer.out items-buyer2.txt > buyer2.log 2>&1 &
pid_buyer2=$!

echo "Starting buyer 3..."
./buyer.out items-buyer3.txt > buyer3.log 2>&1 &
pid_buyer3=$!

echo "Sleeping for 10 seconds..."
sleep 10

echo "Terminating programs..."
kill -9 $pid_seller1
kill -9 $pid_seller2
kill -9 $pid_buyer1
kill -9 $pid_buyer2
kill -9 $pid_buyer3
kill -9 $pid_server

echo "Done."