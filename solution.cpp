#include <cstdio>
#include <string_view>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <thread>
#include <future>
#include <bitset>

//POSIX LIBRARIES
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

using namespace std;

#undef TEST

constexpr int THREADS = 15;

struct row {
    int max, min, sum, count;

    void print() const {
        auto avg = sum/count;
        printf("%d.%d/%d.%d/%d.%d", min/10, min%10, avg/10, avg%10, max/10, max%10);
    }
};

using hash_table = unordered_map<string, row>;

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

hash_table getTableFromChunk(string_view buffer) {
    hash_table m;
    m.reserve(10000);
    const auto sz = buffer.size();

    size_t curr = 0L;
    while(curr < sz) {
        auto next = buffer.find('\n', curr);
        if(next == string_view::npos) next = sz;
        string_view buf(buffer.data() + curr, next-curr);

        size_t i = 0L; 
        while(buf[i] != ';') i++;
        auto last = i;
        i++;

        auto tmp = 0;
        for(; i < buf.size(); ++i) {
            if('0' <= buf[i] && buf[i] <= '9') {
                tmp *= 10;
                tmp += buf[i] - '0';
            }
        }

        auto name = string{ buf.substr(0, last) };
        if(m.count(name)) {
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

        curr = next + 1;
    }

    return m;
}

int main() {

#ifdef TEST
    int fd = ::open("data/dummy.txt", O_RDONLY);
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

    const auto chunks = splitInChunks(data, sz);
    
    hash_table m;
    bitset<THREADS> done;
    m.reserve(10000);

    array<future<hash_table>, THREADS> futures;
    for(int i=0; i<THREADS; ++i) {
        if(!chunks[i].empty()) {
            futures[i] = async(getTableFromChunk, chunks[i]);
            done[i] = false;
        } else {
            done[i] = true;
        }
    }

    while(!done.all()) {
        for(int i=0; i<THREADS; ++i) {
            if(!done[i] &&
            futures[i].wait_until(chrono::system_clock::time_point::min()) == std::future_status::ready) {
                done[i] = true;  
                auto res = futures[i].get();
                printf("%d finished, %lu rows\n", i, res.size());
                m.insert(res.begin(), res.end());
            }
        }
    }

    printf("{");
    for(auto [name, r]: m) {
        printf("%s ", name.c_str());
        r.print();
        printf(", ");
    }
    printf("}");

    return 0;
}