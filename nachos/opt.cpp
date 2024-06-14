#include <iostream>
#include <vector> 
#include <cassert>
using namespace std;
int main(){
    vector<int> traza;
    vector<int> paginas[1000];
    int p;
    int r = 0;
    int mem[32];
    for(int i = 0; i < 32; i++){
        mem[i] = -1;
    }
    while(cin >> p){
        traza.push_back(p);
       // cout << p << " ";
    }
    int sz = traza.size();
    for(int i = sz; i > 0; i--){
        paginas[traza[i-1]].push_back(i-1);
    }
    for(int i = 0; i < sz; i++){
        assert(paginas[traza[i]].back() == i);
        paginas[traza[i]].pop_back();
        int found = 0;
        for(int j = 0; j < 32; j++){
            if(mem[j] == traza[i]) {found = 1; break;}
            if(mem[j] == -1) {
                mem[j] = traza[i];
                found = 1;
                break;
            }
        }
        if(found == 0){
            //busco de la memoria el que mas lejos se ejecuta
            int mint = 0;
            int minp = 0;
            for(int j = 0; j < 32; j++){
                if(paginas[mem[j]].size() == 0){
                    minp = j;
                    break;
                }
                if(paginas[mem[j]].back() > mint){
                    mint = paginas[mem[j]].back();
                    minp = j;
                }
            }
            mem[minp] = traza[i];
            cout << minp << ", ";
            r++;
        }
    }
    cout << r << " " << sz << endl;
    for(int i = 0; i < 100; i++){
        assert(paginas[i].size() == 0);
    }
    return 0;

}