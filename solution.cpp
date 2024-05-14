#include <bits/stdc++.h>
using namespace std;

#undef TEST

struct row {
    int max, min, sum, count;

    void print(int a) const {
        cout << (a/10) << '.' << (a%10);
    }

    void print() const {
        print(min);
        cout << '/';
        print(sum/count);
        cout << '/';
        print(max);
    }
};

int main(int argc, char const *argv[]) {
    unordered_map<string, row> m;
#ifdef TEST
    ifstream in("data/dummy.txt");
#else
    ifstream in("data/measurements.txt");
#endif

    string buf;
    while(getline(in, buf)) {
        int i=0; 
        while(buf[i] != ';') i++;
        int last = i;
        i++;

        int tmp = 0;
        for(; i < buf.size(); ++i) {
            tmp *= 10;
            tmp += buf[i] - '0';
        }

        auto name = buf.substr(0, last);
        if(m.contains(name)) {
            auto& p = m[name];
            p.count++;
            p.sum += tmp;
            p.min = min(p.min, tmp);
            p.max = max(p.max, tmp);
        } else {
            m.emplace(make_pair(name, row {
                tmp, tmp, tmp, 1
            }));
        }
    }

    for(auto [name, r]: m) {
        cout << name << ' ';
        r.print();
        cout << ", ";
    }

    return 0;
}