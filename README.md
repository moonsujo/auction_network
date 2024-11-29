# Description
a server orchestrates placing bids on a seller thread's items from a buyer thread using TCP, UDP, and mutex  
you can run this code by executing "make run-phase2" on the linux command line.

# Instructions (Windows 10 Ubuntu)
clone repository  
sudo apt-get update  
sudo apt-get install g++  
sudo apt-get install make  
cd /mnt/c (c drive)  
(navigate to repository)  
sudo apt-get install dos2unix  
dos2unix run-phase2.sh  
make run-phase2  
  
to re-run the program, make sure to clean the .out files and kill processes from the previous run.   
to check for processes and kill them, run:  
ps aux | grep -e server.out -e seller.out -e buyer.out  
kill -9 <process id>  
to remove the .out files, run:  
make clean  
