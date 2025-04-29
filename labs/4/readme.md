gcc master.c -o master -lrt -lpthread


gcc worker.c -o worker -lrt


./master