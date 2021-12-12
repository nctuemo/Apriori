all: apriori apriori_mpi

apriori : apriori.cpp
	g++ -std=c++11 -o apriori apriori.cpp

apriori_mpi : apriori.cpp
	g++ -std=c++11 -o apriori_mpi apriori_mpi.cpp

clean: 
	rm apriori apriori_mpi