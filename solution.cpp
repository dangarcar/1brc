#include <string_view>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <vector>
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
//#define DEBUG_PRINT

constexpr int THREADS = 15;
constexpr int MAX_STR_LENGTH = 128;
constexpr int MAX_ROWS = 16384; // 2^14
constexpr int HASH_MASK = (1 << 14) -1;

struct row {
    int max, min, sum, count;
    char name[MAX_STR_LENGTH];

    void print() const {
        printf("%s ", name);
        auto avg = sum/count;
        printf("%d.%d/%d.%d/%d.%d", min/10, min%10, avg/10, avg%10, max/10, max%10);
    }
};

struct hash_table {
    array<vector<row>, MAX_ROWS> buckets;
    size_t size=0;

    row* get(int hash, const char* str) {
        for(int j=0; j<buckets[hash].size(); ++j) {
            int cmp=0;

            int i=0;
            while(str[i] != ';') {
                cmp |= str[i] - buckets[hash][j].name[i];
                ++i;
            }

            if(!cmp)
                return &buckets[hash][j];
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

        buckets[hash].push_back(r);

        size++;
    }

    void printUnsorted() const {
        int collisions = 0;
        int holes = 0;

        printf("{");
        for(int i=0; i<MAX_ROWS; ++i) {
            auto b = buckets[i];
            if(!b.empty()) {
                collisions += b.size() > 1;
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

    void printCollisions() const {
        unordered_map<int,int> colls;

        for(int i=0; i<MAX_ROWS; ++i) {
            auto b = buckets[i];
            colls[b.size()]++;
        }
        for(auto [num, count]: colls) {
            if(num > 1)
                printf("%d: %d\n", num, count);
        }

        printf("Collisions: %d\n", MAX_ROWS - colls[0] - colls[1]);
    }

    void insertVector(int hash, vector<row> batch) {
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

            buckets[hash].push_back(b);
            
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

unique_ptr<hash_table> getTableFromChunk(string_view buffer) {
    auto m = make_unique<hash_table>();
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

        auto tmp = 0;
        while(str[i] != '\n') {
            if('0' <= str[i] && str[i] <= '9') {
                tmp *= 10;
                tmp += str[i] - '0';
            }
            ++i;
        }

        auto entry = m->get(hash & HASH_MASK, str);
        if(entry) {
            entry->count++;
            entry->sum += tmp;
            entry->min = min(entry->min, tmp);
            entry->max = max(entry->max, tmp);
        } else {
            m->push(hash & HASH_MASK, str, tmp); 
        }

        curr += i + 1;
    }

    //m->print();

    return m;
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

    bitset<THREADS> done;
    array<future<unique_ptr<hash_table>>, THREADS> futures;
    for(int i=0; i<THREADS; ++i) {
        if(!chunks[i].empty()) {
            futures[i] = async(getTableFromChunk, chunks[i]);
            done[i] = false;
        } else {
            done[i] = true;
        }
    }

#ifdef DEBUG_PRINT
    auto t2 = chrono::high_resolution_clock::now();
#endif

    array<unique_ptr<hash_table>, THREADS> batches;
    while(!done.all()) {
        for(int i=0; i<THREADS; ++i) {
            if(!done[i] &&
            futures[i].wait_until(chrono::system_clock::time_point::min()) == std::future_status::ready) {
                done[i] = true;
                batches[i] = futures[i].get();
                printf("%d finished, %lu rows\n", i, batches[i]->size);
            }
        }
    }

#ifdef DEBUG_PRINT
    auto t3 = chrono::high_resolution_clock::now();
#endif

    hash_table m;
    vector<row*> output;
    for(int hash=0; hash<MAX_ROWS; ++hash) {
        for(int j=0; j<THREADS; ++j) {
            auto &b = batches[j]->buckets[hash];
            if(!b.empty())
                m.insertVector(hash, move(b));
        }

        auto &bucket = m.buckets[hash];
        for(int i=0; i<bucket.size(); ++i) {
            output.push_back(&bucket[i]);
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
    m.printCollisions();

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
