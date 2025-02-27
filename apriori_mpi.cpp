#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <tuple>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <mpi.h>
using namespace std;

long double round(long double value, int pos){
    long double temp;
    temp = value * pow( 10, pos );
    temp = floor( temp + 0.5 );
    temp *= pow( 10, -pos );
    return temp;
}

class Apriori {
private:
    int nowStep, world_rank, world_size;
    long double minSupport;
    MPI_Status status;
    vector<vector<int> > transactions;
    vector<vector<int> > C, L;
    vector<vector<vector<int> > > frequentSet;
    vector<tuple<vector<int>, vector<int>, long double, long double> > associationRules;
public:
    Apriori (vector<vector<int> > _transactions, long double _minSupport, int _world_rank, int _world_size) {
        nowStep=0;
        minSupport = _minSupport;
        world_rank = _world_rank;
        world_size = _world_size;

        for(auto&row:_transactions){
            sort(row.begin(), row.end());
            transactions.push_back(row);
        }
        frequentSet.push_back({{}});
    }
    
    vector<tuple<vector<int>, vector<int>, long double, long double> > getAssociationRules(){
        return associationRules;
    }
    
    void process() {
        
        while(true) {
            C = generateNextC();
            if(C.size()==0) break;
            nowStep++;
            MPI_Barrier(MPI_COMM_WORLD); //sync C from all processes, then do L
            L = generateL();
            if(world_rank == 0){
                frequentSet.push_back(L);
            }
        }
        if(world_rank == 0){
            for(auto&stepItemSet:frequentSet) {
                for(auto&items:stepItemSet) {
                    generateAssociationRule(items, {}, {}, 0);
                }
            }
        }
        
    }
    
    void generateAssociationRule(vector<int> items, vector<int> X, vector<int> Y, int index) {
        if(index == items.size()) {
            if(X.size()==0 || Y.size() == 0) return;
            long double XYsupport = getSupport(items);
            long double Xsupport = getSupport(X);
            
            if(Xsupport == 0) return;
            
            long double support = (long double)XYsupport;
            long double confidence = (long double)XYsupport/Xsupport*100.0;
            associationRules.push_back({X, Y, support, confidence});
            return;
        }
        
        X.push_back(items[index]);
        generateAssociationRule(items, X, Y, index+1);
        X.pop_back();
        Y.push_back(items[index]);
        generateAssociationRule(items, X, Y, index+1);
    }
    
    vector<int> getElement(vector<vector<int> > itemset) {
        vector<int> element;
        set<int> s;
        for(auto&row:itemset) for(auto&col:row) s.insert(col);
        for(auto iter=s.begin(); iter != s.end(); iter++) element.push_back(*iter);
        return element;
    }
    
    vector<vector<int> > generateNextC() {
        if(nowStep==0) {
            vector<vector<int> > ret;
            
            if(world_rank == 0){  //root get elements of transactions 
                vector<int> element = getElement(transactions);
                for(auto&i:element) {
                    ret.push_back(vector<int>(1, i));
                }
                if(world_size>1){  //send elements to workers
                    int send_size = ret.size();
                    
                    for (int i=1; i<world_size; i++){
                        MPI_Send(&send_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                        MPI_Send(&ret[0], send_size, MPI_INT, i, 0, MPI_COMM_WORLD);
                    }
                }
                return ret;
            }
            else {  //recv elements from root
                int recv_size;
                MPI_Recv(&recv_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
                ret.resize(recv_size);
                MPI_Recv(&ret[0], recv_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
                
                return ret;
            }
        } else {
            return pruning(joining());
        }
    }
    
    vector<vector<int> > joining () {
        vector<vector<int> > ret;
        for(int i=0;i<L.size();i++){
            for(int j=i+1;j<L.size(); j++) {
                int k;
                for(k=0;k<nowStep-1; k++) {
                    if(L[i][k] != L[j][k]) break;
                }
                if(k == nowStep-1) {
                    vector<int> tmp;
                    for(int k=0;k<nowStep-1; k++) {
                        tmp.push_back(L[i][k]);
                    }
                    int a = L[i][nowStep-1];
                    int b = L[j][nowStep-1];
                    if(a>b) swap(a,b);
                    tmp.push_back(a), tmp.push_back(b);
                    ret.push_back(tmp);
                }
            }
        }
        return ret;
    }
    
    vector<vector<int> > pruning (vector<vector<int> > joined) {
        vector<vector<int> > ret;
        
        set<vector<int> > lSet;
        for(auto&row:L) lSet.insert(row);
        
        for(auto&row:joined){
            int i;
            for(i=0;i<row.size();i++){
                vector<int> tmp = row;
                tmp.erase(tmp.begin()+i);
                if(lSet.find(tmp) == lSet.end()) {
                    break;
                }
            }
            if(i==row.size()){
                ret.push_back(row);
            }
        }
        return ret;
    }
    
    long double getSupport(vector<int> item) {
        int ret = 0;
        for(auto&row:transactions){
            int i, j;
            if(row.size() < item.size()) continue;
            for(i=0, j=0; i < row.size();i++) {
                if(j==item.size()) break;
                if(row[i] == item[j]) j++;
            }
            if(j==item.size()){
                ret++;
            }
        }
        return (long double)ret/transactions.size()*100.0;
    }
    
    vector<vector<int> > generateL() {
        vector<vector<int> > ret;
        int world_rank, world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        MPI_Status status;
        for(auto&row:C){ //TODO: distribute row works by process rank
            long double support = getSupport(row);
            if(round(support, 2) < minSupport) continue;
            ret.push_back(row);
        }
        if (world_rank != 0){
            //send local ret to root, recv global L
        }
        else {
            //recv ret, combine to root local and send global L
        }
        
        return ret;
    }
};

class InputReader {
private:
    ifstream fin;
    vector<vector<int> > transactions;
public:
    InputReader(string filename) {
        fin.open(filename);
        if(!fin) {
            cout << "Input file could not be opened\n";
            exit(0);
        }
        parse();
    }
    void parse() {
        string str;
        while(!getline(fin, str).eof()){
            vector<int> arr;
            int pre = 0;
            for(int i=0;i<str.size();i++){
                if(str[i] == '\t') {
                    int num = atoi(str.substr(pre, i).c_str());
                    arr.push_back(num);
                    pre = i+1;
                }
            }
            int num = atoi(str.substr(pre, str.size()).c_str());
            arr.push_back(num);
            
            transactions.push_back(arr);
        }
    }
    vector<vector<int> > getTransactions() {
        return transactions;
    }
};

class OutputPrinter {
private:
    ofstream fout;
    vector<tuple<vector<int>, vector<int>, long double, long double> > associationRules;
public:
    OutputPrinter(string filename, vector<tuple<vector<int>, vector<int>, long double, long double> > _associationRules) {
        fout.open(filename);
        if(!fout) {
            cout << "Ouput file could not be opened\n";
            exit(0);
        }
        associationRules = _associationRules;
        buildOutput();
    }
    
    void buildOutput() {
        for(auto&i:associationRules) {
            fout << vectorToString(get<0>(i)) << '\t';
            fout << vectorToString(get<1>(i)) << '\t';
            
            fout << fixed;
            fout.precision(2);
            fout << get<2>(i) << '\t';
            
            fout << fixed;
            fout.precision(2);
            fout << get<3>(i);
            
            fout << endl;
        }
    }
    
    string vectorToString(vector<int> arr) {
        string ret = "{";
        for(int i=0;i<arr.size();i++){
            ret += to_string(arr[i]);
            if(i != arr.size()-1){
                ret += ",";
            }
        }
        ret += "}";
        return ret;
    }
};

class Checker {
public:
    ifstream fin1, fin2;
    set<string> s1;
    Checker(string filename1, string filename2) {
        fin1.open(filename1);
        fin2.open(filename2);
        
        if(!fin1 || !fin2) {
            cout << "Input file could not be opened\n";
            exit(0);
        }
    }
    void compare() {
        file1ToSet();
        
        string str;
        while(!getline(fin2, str).eof()){
            str.pop_back();
            if(s1.find(str) == s1.end()) {
                cout << "failed at " << str <<  endl;
                return;
            }
        }
    }
    void file1ToSet() {
        string str;
        while(!getline(fin1, str).eof()){
            s1.insert(str);
        }
    }
};

int main (int argc, char ** argv) {
    if(argc!=4) {
        cout << "error : The number of parameters must be 3";
        return 0;
    }
    string minSupport(argv[1]);
    string inputFileName(argv[2]);
    string outputFileName(argv[3]);
    int world_rank, world_size;  
    /*
    vector<vector<int> > transactions = {
        {1, 2, 5},
        {2,4},
        {2,3},
        {1, 2, 4},
        {1, 3},
        {2, 3},
        {1, 3},
        {1, 2, 3, 5},
        {1, 2, 3}
    };
    */
    
    InputReader inputReader(inputFileName);
    vector<vector<int> > transactions = inputReader.getTransactions();

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Status status;

    Apriori apriori(transactions, stold(minSupport), world_rank, world_size);
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    apriori.process();
    chrono::steady_clock::time_point end = chrono::steady_clock::now();

    if (world_rank == 0)
    {
    cout << chrono::duration_cast<chrono::microseconds>(end - begin).count() << endl;
    OutputPrinter outputPrinter(outputFileName, apriori.getAssociationRules());
    }
    MPI_Finalize();
    /*
    for test
    Checker checker("output5.txt", "outputRsupport5.txt");
    checker.compare();
    */

    
    return 0;
}
