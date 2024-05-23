#include <string_view>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <vector>
#include <forward_list>
#include <thread>
#include <bitset>
#include <future>
#include <chrono>

//C LIBRARIES
#include <cstdio>
#include <cstring>
#include <cassert>

//POSIX LIBRARIES
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

using namespace std;

//#define TEST
#define DEBUG_PRINT

constexpr int THREADS = 15;
constexpr int MAX_STR_LENGTH = 100;
constexpr int MAX_ROWS = 16384; // 2^14
constexpr int HASH_MASK = (1 << 14) -1;

atomic_int finishedCounter;

struct row {
    char name[MAX_STR_LENGTH];
    int32_t sum, count;
    int16_t max, min;

    void print() const {
        printf("%s ", name);
        auto avg = sum/count;
        printf("%d.%d/%d.%d/%d.%d", min/10, min%10, avg/10, avg%10, max/10, max%10);
    }
};

struct hash_table {
    array<forward_list<row>, MAX_ROWS> buckets;
    size_t size=0;

    row* get(int hash, const char* str) {
        for(row &r: buckets[hash]) {
            int cmp=0;

            int i=0;
            while(str[i] != ';') {
                cmp |= str[i] - r.name[i];
                ++i;
            }

            if(!cmp)
                return &r;
        }

        return nullptr;
    }

    void push(int hash, const char* str, int val) {
        row r;
        r.max = r.min = r.sum = val;
        r.count = 1;
        memset(r.name, 0, MAX_STR_LENGTH);

        int i=0;
        while(str[i] != ';') {
            r.name[i] = str[i];
            ++i;
        }

        buckets[hash].push_front(r);

        size++;
    }

    void printUnsorted() const {
        int collisions = 0;
        int holes = 0;

        printf("{");
        for(int i=0; i<MAX_ROWS; ++i) {
            auto b = buckets[i];
            if(!b.empty()) {
                //collisions += b.size() > 1;
                for(auto r: b) {
                    r.print();
                    printf(", ");
                }
            } else {
                holes += b.empty();
            }
        }
        printf("}");

        printf("\nCollisions: %d\nHoles: %d\n", collisions, holes);
    }

    void insertList(int hash, forward_list<row> batch) {
        for(auto b: batch) {
            for(auto &e: buckets[hash]) {
                int cmp = strncmp(b.name, e.name, MAX_STR_LENGTH);

                if(cmp == 0) {
                    e.count += b.count;
                    e.sum += b.sum;
                    e.max = max(e.max, b.max);
                    e.min = min(e.min, b.min);
                    goto sum_end;
                }
            }

            buckets[hash].push_front(b);
            
            sum_end:;
        }
    }

private:

};

inline array<string_view, THREADS> splitInChunks(const char* data, size_t sz) {
    const auto npos = data + sz;
    const auto chunkSize = sz/THREADS;
    
    array<string_view, THREADS> chunks{};
    auto curr = data;
    for(int i=0; i<THREADS; ++i) {
        if(curr >= npos) break;

        auto end = find(curr + chunkSize, npos, '\n');
        chunks[i] = string_view(curr, end);
        curr = end + 1;
    }

    return chunks;
}

void getTableFromChunk(string_view buffer, hash_table& m) {
    const auto sz = buffer.size();

    size_t curr = 0L;
    while(curr < sz) {
        auto str = buffer.data() + curr;
        int hash = 0;

        size_t i = 0L; 
        while(str[i] != ';') {
            hash = ((hash << 5) + hash) ^ str[i];
            i++;
        }
        auto last = i;
        i++;

        int16_t tmp = 0;
        while(str[i] != '\n') {
            if('0' <= str[i] && str[i] <= '9') {
                tmp *= 10;
                tmp += str[i] - '0';
            }
            ++i;
        }

        auto entry = m.get(hash & HASH_MASK, str);
        if(entry) {
            entry->count++;
            entry->sum += tmp;
            entry->min = min(entry->min, tmp);
            entry->max = max(entry->max, tmp);
        } else {
            m.push(hash & HASH_MASK, str, tmp); 
        }

        curr += i + 1;
    }

    //m.print();
    //return m;
}

int main() {
#ifdef DEBUG_PRINT
    auto t0 = chrono::high_resolution_clock::now();
#endif

#ifdef TEST
    int fd = ::open("data/test.txt", O_RDONLY);
#else
    int fd = ::open("data/measurements.txt", O_RDONLY);
#endif
    
    if(!fd) {
        perror("Error opening data");
        exit(1);
    }

    struct stat sb;
    if(fstat(fd, &sb) == -1) {
        perror("Error getting file size");
        exit(1);
    }

    auto sz = sb.st_size;
    const char* data = (const char*) mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    if(data == MAP_FAILED) {
        perror("Error mapping file");
        exit(1);
    }
    
#ifdef DEBUG_PRINT
    auto t1 = chrono::high_resolution_clock::now();
#endif

    const auto chunks = splitInChunks(data, sz);

    array<hash_table, THREADS> tables;
    for(int i=0; i<THREADS; ++i) {
        if(!chunks[i].empty()) {
            auto t = thread([&, i]{
                getTableFromChunk(chunks[i], tables[i]);
                finishedCounter++;
            #ifdef DEBUG_PRINT
                printf("%d finished, %lu rows\n", i, tables[i].size);
            #endif
            });
            t.detach();
        } else {
            finishedCounter++;
        }
    }

#ifdef DEBUG_PRINT
    auto t2 = chrono::high_resolution_clock::now();
#endif

    while(finishedCounter < THREADS);

#ifdef DEBUG_PRINT
    auto t3 = chrono::high_resolution_clock::now();
#endif

    auto& m = tables[0];
    vector<row*> output;
    for(int j=1; j<THREADS; ++j) {
        for(int hash=0; hash<MAX_ROWS; ++hash) {
            auto &b = tables[j].buckets[hash];
            if(!b.empty())
                m.insertList(hash, move(b));
        }
    }

    for(auto& bucket: m.buckets) {
        for(row& r: bucket) {
            output.push_back(&r);
        }
    }

    sort(output.begin(), output.end(), [](row* a, row* b) {
        return strncmp(a->name, b->name, MAX_STR_LENGTH) < 0;
    });

    printf("{");
    for(int i=0; i<output.size(); ++i) {
        output[i]->print();
        if(i < output.size()-1)
            printf(", ");
    }
    printf("}\n");

#ifdef DEBUG_PRINT
    //m.printCollisions();

    auto t4 = chrono::high_resolution_clock::now();

    printf("\n\
Load file: %li ms\n\
Chunks initialization; %li ms\n\
Thread time: %li ms\n\
Print results: %li ms\n\
Total: %li ms\n", 
        (t1-t0).count()/1000000,
        (t2-t1).count()/1000000,
        (t3-t2).count()/1000000,
        (t4-t3).count()/1000000,
        (t4-t0).count()/1000000
    );
#endif

    return 0;
}
