all: apriori apriori_mpi

apriori : apriori.cpp
	g++ -std=c++11 -o apriori apriori.cpp

apriori_mpi : apriori_mpi.cpp
	mpicxx apriori_mpi.cpp -o apriori_mpi

clean: 
	rm apriori apriori_mpi